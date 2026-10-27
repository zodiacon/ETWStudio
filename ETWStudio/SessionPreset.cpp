#include "pch.h"
#include "SessionPreset.h"
#include "StringHelper.h"
#include <KernelEvents.h>

namespace {
	const PCWSTR FileHeader = L"ETWStudio Session 1";

	std::vector<std::wstring> Split(std::wstring const& text, size_t maxParts) {
		std::vector<std::wstring> parts;
		size_t start = 0;
		while (parts.size() + 1 < maxParts) {
			auto tab = text.find(L'\t', start);
			if (tab == std::wstring::npos)
				break;
			parts.push_back(text.substr(start, tab - start));
			start = tab + 1;
		}
		parts.push_back(text.substr(start));	// the last part may contain tabs
		return parts;
	}

	ULONG ToULong(std::wstring const& text) {
		return std::wcstoul(text.c_str(), nullptr, 10);
	}

	ULONGLONG HexToULongLong(std::wstring const& text) {
		return std::wcstoull(text.c_str(), nullptr, 16);
	}
}

SessionPreset SessionPreset::FromSession(TraceSession const& session) {
	SessionPreset preset;
	preset.Name = session.SessionName();
	preset.OutputFile = session.OutputFile();
	preset.Buffers = session.GetBufferSettings();
	for (auto& [guid, config] : session.GetProviders())
		preset.Providers.push_back({ guid, StringHelper::ProviderGuidToName(guid), config, session.GetEventsForProvider(guid) });

	// stacks are kept per category name
	auto stacks = session.GetKernelEventStacks();
	for (auto type : session.GetKernelEventTypes()) {
		Kernel kernel{ type };
		for (auto& cat : KernelEventCategory::GetAllCategories())
			if (cat.EnableFlag == type && std::ranges::find(stacks, cat.Name) != stacks.end())
				kernel.Stacks = true;
		preset.KernelTypes.push_back(kernel);
	}
	return preset;
}

bool SessionPreset::Save(PCWSTR path) const {
	FILE* fp;
	if (::_wfopen_s(&fp, path, L"w, ccs=UTF-8") != 0)
		return false;
	fwprintf(fp, L"%s\n", FileHeader);
	fwprintf(fp, L"Name\t%s\n", Name.c_str());
	if (!OutputFile.empty())
		fwprintf(fp, L"OutputFile\t%s\n", OutputFile.c_str());
	fwprintf(fp, L"Buffers\t%u\t%u\t%u\t%u\n", Buffers.BufferSizeKB, Buffers.MinimumBuffers, Buffers.MaximumBuffers, Buffers.FlushTimerSec);
	for (auto& p : Providers) {
		std::wstring ids;
		for (auto id : p.EventIds)
			ids += (ids.empty() ? L"" : L",") + std::to_wstring(id);
		fwprintf(fp, L"Provider\t%s\t%d\t%llX\t%llX\t%d\t%s\t%s\n", StringHelper::GuidToString(p.Guid).c_str(), p.Config.Level,
			p.Config.MatchAnyKeyword, p.Config.MatchAllKeyword, p.Config.Stacks ? 1 : 0, ids.c_str(), p.Name.c_str());
	}
	for (auto& k : KernelTypes)
		fwprintf(fp, L"Kernel\t%llX\t%d\n", (ULONGLONG)k.Type, k.Stacks ? 1 : 0);
	bool ok = !ferror(fp);
	fclose(fp);
	return ok;
}

std::optional<SessionPreset> SessionPreset::Load(PCWSTR path) {
	FILE* fp;
	if (::_wfopen_s(&fp, path, L"r, ccs=UTF-8") != 0)
		return {};

	SessionPreset preset;
	WCHAR line[4096];
	bool ok = fgetws(line, _countof(line), fp) && std::wstring_view(line).starts_with(FileHeader);
	while (ok && fgetws(line, _countof(line), fp)) {
		std::wstring text(line);
		while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r'))
			text.pop_back();
		if (text.empty())
			continue;

		auto parts = Split(text, 8);
		auto& kind = parts[0];
		if (kind == L"Name" && parts.size() == 2)
			preset.Name = parts[1];
		else if (kind == L"OutputFile" && parts.size() == 2)
			preset.OutputFile = parts[1];
		else if (kind == L"Buffers" && parts.size() == 5)
			preset.Buffers = { ToULong(parts[1]), ToULong(parts[2]), ToULong(parts[3]), ToULong(parts[4]) };
		else if (kind == L"Provider" && parts.size() == 8) {
			Provider p;
			if (S_OK != ::CLSIDFromString(parts[1].c_str(), &p.Guid)) {
				ok = false;
				break;
			}
			p.Config.Level = (int)ToULong(parts[2]);
			p.Config.MatchAnyKeyword = HexToULongLong(parts[3]);
			p.Config.MatchAllKeyword = HexToULongLong(parts[4]);
			p.Config.Stacks = parts[5] == L"1";
			for (size_t start = 0; start < parts[6].size();) {
				auto comma = parts[6].find(L',', start);
				auto id = ToULong(parts[6].substr(start, comma - start));
				if (id <= 0xffff)
					p.EventIds.push_back((USHORT)id);
				start = comma == std::wstring::npos ? parts[6].size() : comma + 1;
			}
			p.Name = parts[7];
			preset.Providers.push_back(std::move(p));
		}
		else if (kind == L"Kernel" && parts.size() == 3)
			preset.KernelTypes.push_back({ (KernelEventTypes)HexToULongLong(parts[1]), parts[2] == L"1" });
		else
			ok = false;
	}
	fclose(fp);
	if (!ok)
		return {};
	return preset;
}
