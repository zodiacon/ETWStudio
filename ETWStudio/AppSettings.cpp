#include "pch.h"
#include "AppSettings.h"

namespace {
	std::wstring Trim(std::wstring const& s, std::wstring const& chars = L" \t\r\n") {
		auto first = s.find_first_not_of(chars);
		if (first == std::wstring::npos)
			return {};
		return s.substr(first, s.find_last_not_of(chars) - first + 1);
	}
}

void AppSettings::Upgrade() {
	// the old single path replaced _NT_SYMBOL_PATH entirely
	if (auto path = SymbolPath(); !path.empty()) {
		SymbolExtraPaths(path);
		SymbolUseEnvPath(false);
		SymbolPath(L"");
	}
	// an earlier build saved the defaults' pointers instead of their text
	if (auto url = SymbolServerUrl(); _wcsnicmp(url.c_str(), L"http://", 7) != 0 && _wcsnicmp(url.c_str(), L"https://", 8) != 0)
		SymbolServerUrl(DefaultSymbolServerUrl);
	if (auto cache = SymbolCache(); !cache.empty() && cache.find_first_of(L":\\%") == std::wstring::npos)
		SymbolCache(DefaultSymbolCache);
}

std::vector<std::wstring> AppSettings::RecentSessionList() const {
	std::vector<std::wstring> paths;
	auto text = RecentSessions();
	for (size_t start = 0; start < text.size(); ) {
		auto end = text.find(L'\n', start);
		if (end == std::wstring::npos)
			end = text.size();
		if (end > start && paths.size() < MaxRecentSessions)
			paths.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	return paths;
}

void AppSettings::UpdateRecentSessions(std::wstring const& path, bool remove) {
	auto paths = RecentSessionList();
	std::erase_if(paths, [&](auto const& p) { return ::_wcsicmp(p.c_str(), path.c_str()) == 0; });
	if (!remove)
		paths.insert(paths.begin(), path);
	std::wstring text;
	for (size_t i = 0; i < paths.size() && i < MaxRecentSessions; i++)
		text += (i ? L"\n" : L"") + paths[i];
	RecentSessions(text);
}

std::wstring AppSettings::BuildSymbolSearchPath() const {
	return BuildSymbolSearchPath(SymbolServerEnabled(), SymbolServerUrl(), SymbolCache(), SymbolExtraPaths(), SymbolUseEnvPath());
}

std::wstring AppSettings::BuildSymbolSearchPath(bool useServer, std::wstring const& url, std::wstring const& cache,
	std::wstring const& extra, bool useEnv) {
	std::wstring path;
	auto add = [&](std::wstring const& part) {
		if (part.empty())
			return;
		if (!path.empty())
			path += L";";
		path += part;
	};

	// local locations first, so the server is only consulted when they have nothing
	add(Trim(extra, L" \t\r\n;"));
	if (useEnv) {
		WCHAR env[4096];
		auto len = ::GetEnvironmentVariable(L"_NT_SYMBOL_PATH", env, _countof(env));
		if (len > 0 && len < _countof(env))
			add(Trim(env, L" \t\r\n;"));
	}
	auto server = Trim(url);
	if (useServer && !server.empty()) {
		auto dir = Trim(cache);
		if (!dir.empty()) {
			WCHAR expanded[MAX_PATH];
			auto len = ::ExpandEnvironmentStrings(dir.c_str(), expanded, _countof(expanded));
			if (len > 0 && len <= _countof(expanded))
				dir = expanded;
		}
		add(dir.empty() ? L"srv*" + server : L"srv*" + dir + L"*" + server);
	}
	return path;
}
