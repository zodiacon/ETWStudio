#include "pch.h"
#include "EtwSession.h"

EtwSession::EtwSession(std::wstring name) : m_Name(std::move(name)) {
}

void EtwSession::ClearProviders() {
    m_Providers.clear();
}

std::span<const ProviderSettings> EtwSession::GetProviders() const {
    return std::span(m_Providers);
}

std::wstring const& EtwSession::Name() const {
    return m_Name;
}
