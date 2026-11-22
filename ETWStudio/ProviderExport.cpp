#include "pch.h"
#include "ProviderExport.h"
#include "EventExport.h"
#include "StringHelper.h"

using EventExport::CsvField;
using EventExport::JsonString;

namespace {
	// TDH names often end with a space
	std::wstring_view Trim(std::wstring const& text) {
		std::wstring_view view(text);
		while (!view.empty() && iswspace(view.back()))
			view.remove_suffix(1);
		return view;
	}

	PCWSTR SourceName(EtwSchemaSource source) {
		return source == EtwSchemaSource::Mof ? L"MOF" : L"XML";
	}

	// the provider's events in a stable order (so exports from different machines/builds diff well)
	std::vector<EVENT_DESCRIPTOR> SortedEvents(EtwProvider const& provider) {
		auto events = provider.GetProviderEvents();
		std::ranges::sort(events, [](auto const& e1, auto const& e2) {
			return e1.Id != e2.Id ? e1.Id < e2.Id : e1.Version < e2.Version;
			});
		return events;
	}

	// "Name (InType); Name2 (InType2)"
	std::wstring PropertyList(std::vector<EtwEventProperty> const& properties) {
		std::wstring text;
		for (auto const& prop : properties) {
			if (!text.empty())
				text += L"; ";
			text += std::format(L"{} ({})", prop.Name, StringHelper::InTypeToString(prop.InType));
		}
		return text;
	}

	constexpr PCWSTR EventCsvHeader = L"ID,Version,Level,Level Name,Keyword,Keyword Name,Task,Task Name,Opcode,Opcode Name,"
		L"Channel,Channel Name,Name,Message,Properties";

	// the numbers come from the descriptor (the event info's is empty when its lookup failed)
	void WriteEventCsvRow(Utf8Writer& writer, EtwProvider const& provider, EVENT_DESCRIPTOR const& desc) {
		auto const& info = provider.EventInfo(desc);
		writer << std::format(L"{},{},{},", desc.Id, desc.Version, desc.Level) << CsvField(Trim(info.LevelName))
			<< std::format(L",0x{:X},", desc.Keyword) << CsvField(Trim(info.KeywordName))
			<< std::format(L",{},", desc.Task) << CsvField(Trim(info.TaskName))
			<< std::format(L",{},", desc.Opcode) << CsvField(Trim(info.OpCodeName))
			<< std::format(L",{},", desc.Channel) << CsvField(Trim(info.ChannelName))
			<< L"," << CsvField(Trim(info.EventName)) << L"," << CsvField(Trim(info.EventMessage))
			<< L"," << CsvField(PropertyList(info.Properties)) << L"\r\n";
	}

	void WriteEventJson(Utf8Writer& writer, EtwProvider const& provider, EVENT_DESCRIPTOR const& desc) {
		auto const& info = provider.EventInfo(desc);
		writer << std::format(L"   {{ \"Id\": {}, \"Version\": {}, \"Name\": {}, \"Level\": {}, \"LevelName\": {}, \"Keyword\": \"0x{:X}\", \"KeywordName\": {},\n",
			desc.Id, desc.Version, JsonString(Trim(info.EventName)), desc.Level, JsonString(Trim(info.LevelName)), desc.Keyword, JsonString(Trim(info.KeywordName)));
		writer << std::format(L"     \"Task\": {}, \"TaskName\": {}, \"Opcode\": {}, \"OpcodeName\": {}, \"Channel\": {}, \"ChannelName\": {}, \"Message\": {},\n",
			desc.Task, JsonString(Trim(info.TaskName)), desc.Opcode, JsonString(Trim(info.OpCodeName)), desc.Channel, JsonString(Trim(info.ChannelName)),
			JsonString(Trim(info.EventMessage)));
		writer << L"     \"Properties\": [";
		bool first = true;
		for (auto const& prop : info.Properties) {
			writer << (first ? L" " : L", ") << std::format(L"{{ \"Name\": {}, \"InType\": {}, \"OutType\": {} }}",
				JsonString(prop.Name), JsonString(StringHelper::InTypeToString(prop.InType)), JsonString(StringHelper::OutTypeToString(prop.OutType)));
			first = false;
		}
		writer << (first ? L"] }" : L" ] }");
	}

	// events == nullptr: no events, just their count
	bool WriteProviderObject(Utf8Writer& writer, EtwProvider const& provider, std::span<EVENT_DESCRIPTOR const> const* events,
		ProviderExport::Progress const& progress) {
		writer << L" { \"Name\": " << JsonString(provider.Name()) << L", \"Guid\": " << JsonString(provider.GuidAsString())
			<< L", \"Source\": " << JsonString(SourceName(provider.SchemaSource()));
		if (!events) {
			writer << std::format(L", \"EventCount\": {} }}", provider.EventCount());
			return true;
		}
		writer << std::format(L", \"EventCount\": {},\n  \"Events\": [", events->size());
		for (size_t i = 0; i < events->size(); i++) {
			if (progress && i % 64 == 0 && !progress(i))
				return false;
			writer << (i ? L",\n" : L"\n");
			WriteEventJson(writer, provider, (*events)[i]);
		}
		writer << (events->empty() ? L"] }" : L"\n  ] }");
		return true;
	}
}

bool ProviderExport::WriteProviderListCsv(Utf8Writer& writer, std::span<EtwProvider const> providers, Progress const& progress) {
	writer << L"\xFEFF";	// BOM, so Excel detects UTF-8
	writer << L"Name,GUID,Source,Events\r\n";
	for (size_t i = 0; i < providers.size(); i++) {
		if (progress && i % 64 == 0 && !progress(i))
			return false;
		auto const& p = providers[i];
		writer << CsvField(p.Name()) << L"," << p.GuidAsString() << L"," << SourceName(p.SchemaSource())
			<< std::format(L",{}\r\n", p.EventCount());
	}
	return writer.Ok();
}

bool ProviderExport::WriteEventsCsv(Utf8Writer& writer, EtwProvider const& provider, std::span<EVENT_DESCRIPTOR const> events,
	Progress const& progress) {
	writer << L"\xFEFF" << EventCsvHeader << L"\r\n";
	for (size_t i = 0; i < events.size(); i++) {
		if (progress && i % 64 == 0 && !progress(i))
			return false;
		WriteEventCsvRow(writer, provider, events[i]);
	}
	return writer.Ok();
}

bool ProviderExport::WriteAllEventsCsv(Utf8Writer& writer, std::span<EtwProvider const> providers, Progress const& progress) {
	writer << L"\xFEFF" << L"Provider,Provider GUID,Source," << EventCsvHeader << L"\r\n";
	for (size_t i = 0; i < providers.size(); i++) {
		if (progress && !progress(i))
			return false;
		auto const& p = providers[i];
		auto prefix = CsvField(p.Name()) + L"," + p.GuidAsString() + L"," + SourceName(p.SchemaSource()) + L",";
		for (auto const& desc : SortedEvents(p)) {
			writer << prefix;
			WriteEventCsvRow(writer, p, desc);
		}
		if (!writer.Ok())
			return false;
	}
	return writer.Ok();
}

bool ProviderExport::WriteProvidersJson(Utf8Writer& writer, std::span<EtwProvider const> providers, bool events, Progress const& progress) {
	writer << L"[";
	for (size_t i = 0; i < providers.size(); i++) {
		if (progress && (events || i % 64 == 0) && !progress(i))
			return false;
		writer << (i ? L",\n" : L"\n");
		if (events) {
			auto sorted = SortedEvents(providers[i]);
			std::span<EVENT_DESCRIPTOR const> list(sorted);
			WriteProviderObject(writer, providers[i], &list, nullptr);
		}
		else {
			WriteProviderObject(writer, providers[i], nullptr, nullptr);
		}
		if (!writer.Ok())
			return false;
	}
	writer << (providers.empty() ? L"]\n" : L"\n]\n");
	return writer.Ok();
}

bool ProviderExport::WriteProviderJson(Utf8Writer& writer, EtwProvider const& provider, std::span<EVENT_DESCRIPTOR const> events,
	Progress const& progress) {
	if (!WriteProviderObject(writer, provider, &events, progress))
		return false;
	writer << L"\n";
	return writer.Ok();
}
