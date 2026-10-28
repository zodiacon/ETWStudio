#include "pch.h"
#include "EventExport.h"
#include "StringHelper.h"
#include <EventData.h>

namespace {
	std::wstring JsonString(std::wstring_view text) {
		std::wstring json(L"\"");
		for (auto ch : text) {
			switch (ch) {
				case L'"': json += L"\\\""; break;
				case L'\\': json += L"\\\\"; break;
				case L'\n': json += L"\\n"; break;
				case L'\r': json += L"\\r"; break;
				case L'\t': json += L"\\t"; break;
				default:
					if (ch < 0x20)
						json += std::format(L"\\u{:04x}", (unsigned)ch);
					else
						json += ch;
			}
		}
		return json + L'"';
	}

	// ISO 8601, UTC, 100 nsec precision
	std::wstring IsoTime(ULONGLONG ts) {
		FILETIME ft;
		*(ULONGLONG*)&ft = ts;
		SYSTEMTIME st;
		if (!::FileTimeToSystemTime(&ft, &st))
			return L"";
		return std::format(L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:07}Z", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, ts % 10000000);
	}
}

std::wstring EventExport::CsvField(std::wstring_view text) {
	if (text.find_first_of(L",\"\r\n") == std::wstring_view::npos)
		return std::wstring(text);
	std::wstring field(L"\"");
	for (auto ch : text) {
		if (ch == L'"')
			field += L'"';
		field += ch;
	}
	return field + L'"';
}

bool EventExport::WriteJson(Utf8Writer& writer, std::span<EventData* const> events, std::function<bool(size_t)> const& progress,
	StackSymbols const& symbols) {
	writer << L"[";
	bool first = true;
	size_t count = 0;
	for (auto evt : events) {
		if (progress && count++ % 256 == 0 && !progress(count - 1))
			return false;
		auto& desc = evt->GetEventDescriptor();
		auto& strings = evt->GetEventStrings();
		writer << (first ? L"\n  {" : L",\n  {");
		first = false;
		writer << std::format(L"\"Index\": {}, \"Time\": {}, \"ProcessId\": {}, \"ThreadId\": {}, \"ProcessName\": {}, \"CPU\": {},\n",
			evt->GetIndex(), JsonString(IsoTime(evt->GetTimeStamp())), evt->GetProcessId(), evt->GetThreadId(), JsonString(evt->GetProcessName()), evt->GetCPU());
		writer << std::format(L"   \"Provider\": {}, \"ProviderId\": {}, \"EventId\": {}, \"Version\": {}, \"EventName\": {},\n",
			JsonString(StringHelper::ProviderGuidToName(evt->GetProviderId())), JsonString(StringHelper::GuidToString(evt->GetProviderId())),
			desc.Id, desc.Version, JsonString(strings.Name));
		writer << std::format(L"   \"Level\": {}, \"LevelName\": {}, \"Opcode\": {}, \"OpcodeName\": {}, \"Task\": {}, \"TaskName\": {}, \"Keyword\": \"0x{:X}\", \"KeywordName\": {}, \"Channel\": {},\n",
			desc.Level, JsonString(StringHelper::LevelToString(desc.Level)), desc.Opcode, JsonString(strings.Opcode), desc.Task, JsonString(strings.Task),
			desc.Keyword, JsonString(strings.Keyword), JsonString(strings.Channel));
		writer << L"   \"Properties\": {";
		bool firstProp = true;
		for (auto& prop : evt->GetProperties()) {
			writer << (firstProp ? L" " : L", ") << JsonString(prop.Name) << L": " << JsonString(evt->FormatProperty(prop));
			firstProp = false;
		}
		writer << (firstProp ? L"}" : L" }");
		if (evt->HasStackTrace()) {
			writer << L",\n   \"Stack\": [";
			bool firstFrame = true;
			for (auto address : evt->GetStackTrace()) {
				writer << std::format(L"{}\"0x{:X}\"", firstFrame ? L"" : L", ", address);
				firstFrame = false;
			}
			writer << L"]";
			if (symbols) {
				writer << L",\n   \"StackSymbols\": [";
				firstFrame = true;
				for (auto& symbol : symbols(*evt)) {
					writer << (firstFrame ? L"" : L", ") << JsonString(symbol);
					firstFrame = false;
				}
				writer << L"]";
			}
		}
		writer << L" }";
	}
	writer << L"\n]\n";
	return writer.Ok();
}
