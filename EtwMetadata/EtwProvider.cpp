#include "pch.h"
#include "EtwProvider.h"
#include <memory>
#include <algorithm>
#include <assert.h>

#pragma comment(lib, "tdh")

EtwProvider::EtwProvider(GUID const& guid, PCWSTR name, EtwSchemaSource source) : m_guid(guid), m_name(name), m_source(source) {
    WCHAR sguid[64];
    if(::StringFromGUID2(guid, sguid, _countof(sguid)))
        m_guidString = sguid;
}

std::vector<EtwProvider> EtwProvider::EnumProviders(bool sort) {
    std::vector<EtwProvider> providers;

    ULONG size = 0;
    auto error = ::TdhEnumerateProviders(nullptr, &size);
    assert(error == ERROR_INSUFFICIENT_BUFFER);
    auto buffer = std::make_unique<BYTE[]>(size);
    if (!buffer)
        return providers;

    auto data = reinterpret_cast<PROVIDER_ENUMERATION_INFO*>(buffer.get());
    error = ::TdhEnumerateProviders(data, &size);
    assert(error == ERROR_SUCCESS);
    if (error != ERROR_SUCCESS)
        return providers;

    providers.reserve(data->NumberOfProviders);
    for (ULONG i = 0; i < data->NumberOfProviders; i++) {
        const auto& item = data->TraceProviderInfoArray[i];
        EtwProvider provider(item.ProviderGuid, (PCWSTR)(buffer.get() + item.ProviderNameOffset), static_cast<EtwSchemaSource>(item.SchemaSource));
        providers.push_back(std::move(provider));
    }
    if (sort) {
        std::sort(providers.begin(), providers.end(), [](const auto& p1, const auto& p2) {
            return _wcsicmp(p1.Name().c_str(), p2.Name().c_str()) < 0;
            });
    }
    return providers;
}

const std::wstring& EtwProvider::Name() const {
    return m_name;
}

const std::wstring& EtwProvider::GuidAsString() const {
    return m_guidString;
}

const GUID& EtwProvider::Guid() const {
    return m_guid;
}

EtwSchemaSource EtwProvider::SchemaSource() const {
    return m_source;
}

std::vector<EVENT_DESCRIPTOR> const& EtwProvider::GetProviderEvents() const {
    if(m_eventCount < 0) {
        m_eventCount = 0;
        std::vector<EVENT_DESCRIPTOR> events;

        ULONG size = 0;
        auto error = ::TdhEnumerateManifestProviderEvents((LPGUID)&m_guid, nullptr, &size);
        if(error != ERROR_INSUFFICIENT_BUFFER)
            return m_events;

        auto buffer = std::make_unique<BYTE[]>(size);
        if(!buffer)
            return m_events;

        auto data = reinterpret_cast<PROVIDER_EVENT_INFO*>(buffer.get());
        error = ::TdhEnumerateManifestProviderEvents((LPGUID)&m_guid, data, &size);
        assert(error == ERROR_SUCCESS);
        if(error != ERROR_SUCCESS)
            return m_events;

        events.reserve(data->NumberOfEvents);
        for(ULONG i = 0; i < data->NumberOfEvents; i++)
            events.push_back(data->EventDescriptorsArray[i]);
        m_events = std::move(events);
        m_eventCount = static_cast<int32_t>(m_events.size());
    }
    return m_events;
}

EtwEventInfo EtwProvider::EventInfo(const EVENT_DESCRIPTOR& desc) const {
    EtwEventInfo info;
    info.ProviderGuid = GUID_NULL;

    ULONG size = 0;
    ::TdhGetManifestEventInformation((LPGUID)&m_guid, (PEVENT_DESCRIPTOR)&desc, nullptr, &size);
    auto buffer = std::make_unique<BYTE[]>(size);
    if (!buffer)
        return info;

    auto data = reinterpret_cast<TRACE_EVENT_INFO*>(buffer.get());
    auto error = ::TdhGetManifestEventInformation((LPGUID)&m_guid, (PEVENT_DESCRIPTOR)&desc, data, &size);
    assert(error == ERROR_SUCCESS);
    if (ERROR_SUCCESS != error)
        return info;

    info.ProviderGuid = data->ProviderGuid;
    info.EventGuid = data->EventGuid;
    info.Descriptor = desc;
    info.DescodingSource = static_cast<DecodingSource>(data->DecodingSource);

    if (data->EventNameOffset)
        info.EventName = (PCWSTR)(buffer.get() + data->EventNameOffset);
    if (data->ChannelNameOffset)
        info.ChannelName = (PCWSTR)(buffer.get() + data->ChannelNameOffset);
    if (data->KeywordsNameOffset)
        info.KeywordName = (PCWSTR)(buffer.get() + data->KeywordsNameOffset);
    if (data->LevelNameOffset)
        info.LevelName = (PCWSTR)(buffer.get() + data->LevelNameOffset);
    if (data->OpcodeNameOffset)
        info.OpCodeName = (PCWSTR)(buffer.get() + data->OpcodeNameOffset);
    if (data->EventMessageOffset)
        info.EventMessage = (PCWSTR)(buffer.get() + data->EventMessageOffset);
    if (data->TaskNameOffset)
        info.TaskName = (PCWSTR)(buffer.get() + data->TaskNameOffset);
    if (data->ProviderMessageOffset)
        info.ProviderMessage = (PCWSTR)(buffer.get() + data->ProviderMessageOffset);

    for (DWORD i = 0; i < data->PropertyCount; i++) {
        auto& p = data->EventPropertyInfoArray[i];
        EtwEventProperty prop;
        prop.Flags = static_cast<EtwPropertyFlags>(p.Flags);
        prop.Name = (PCWSTR)(buffer.get() + p.NameOffset);
        if ((prop.Flags & EtwPropertyFlags::Struct) == EtwPropertyFlags::None) {
            prop.InType = p.nonStructType.InType;
            prop.OutType = p.nonStructType.OutType;
            if (p.nonStructType.MapNameOffset)
                prop.MapName = (PCWSTR)(buffer.get() + p.nonStructType.MapNameOffset);
        }
        info.Properties.push_back(prop);
    }

    return info;
}

int32_t EtwProvider::EventCount() const {
    if(m_eventCount >= 0)
        return m_eventCount;

    GetProviderEvents();
    return m_eventCount;
}

std::vector<EtwFieldInfo> EtwProvider::FieldInfo(EtwFieldType type) const {
    std::vector<EtwFieldInfo> fields;

    ULONG size = 0;
    if(ERROR_INSUFFICIENT_BUFFER != 
        ::TdhEnumerateProviderFieldInformation((LPGUID)&m_guid, (EVENT_FIELD_TYPE)type, nullptr, &size))
        return fields;

    auto buffer = std::make_unique<BYTE[]>(size);
    auto info = reinterpret_cast<PPROVIDER_FIELD_INFOARRAY>(buffer.get());
    if(ERROR_SUCCESS != ::TdhEnumerateProviderFieldInformation((LPGUID)&m_guid, (EVENT_FIELD_TYPE)type, info, &size))
        return fields;

    fields.reserve(info->NumberOfElements);
    for(ULONG i = 0; i < info->NumberOfElements; i++) {
        auto& item = info->FieldInfoArray[i];
        EtwFieldInfo info;
        info.Value = item.Value;
        if(item.NameOffset)
            info.Name = (PCWSTR)(buffer.get() + item.NameOffset);
        if(item.DescriptionOffset)
            info.Desc = (PCWSTR)(buffer.get() + item.DescriptionOffset);
        fields.push_back(std::move(info));
    }
    return fields;
}
