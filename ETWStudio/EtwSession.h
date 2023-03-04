#pragma once

struct ProviderSettings {
	GUID Guid;
	UCHAR Level;
};

class EtwSession {
public:
	explicit EtwSession(std::wstring name);
	bool AddProvider(GUID const& guid, UCHAR level = 0);
	void ClearProviders();
	std::span<const ProviderSettings> GetProviders() const;
	std::wstring const& Name() const;
	void SetName(std::wstring name);

private:
	std::vector<ProviderSettings> m_Providers;
	std::wstring m_Name;
};

