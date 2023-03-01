#include "pch.h"
#include "EtwProvider.h"
#include <memory>
#include <algorithm>
#include <assert.h>
#include "WMIHelper.h"
#include <wil\com.h>

#pragma comment(lib, "tdh")

EtwProvider::EtwProvider(GUID const& guid, PCWSTR name, EtwSchemaSource source) : m_Guid(guid), m_name(name), m_Source(source) {
	WCHAR sguid[64];
	if (::StringFromGUID2(guid, sguid, _countof(sguid)))
		m_GuidString = sguid;
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

std::vector<std::unique_ptr<EtwProvider>> EtwProvider::EnumProviders2(bool sort) {
	std::vector<std::unique_ptr<EtwProvider>> providers;

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
		auto provider = std::make_unique<EtwProvider>(item.ProviderGuid, (PCWSTR)(buffer.get() + item.ProviderNameOffset), static_cast<EtwSchemaSource>(item.SchemaSource));
		providers.push_back(std::move(provider));
	}
	if (sort) {
		std::sort(providers.begin(), providers.end(), [](const auto& p1, const auto& p2) {
			return _wcsicmp(p1->Name().c_str(), p2->Name().c_str()) < 0;
			});
	}
	return providers;
}

const std::wstring& EtwProvider::Name() const {
	return m_name;
}

const std::wstring& EtwProvider::GuidAsString() const {
	return m_GuidString;
}

const GUID& EtwProvider::Guid() const {
	return m_Guid;
}

EtwSchemaSource EtwProvider::SchemaSource() const {
	return m_Source;
}

std::vector<EVENT_DESCRIPTOR> const& EtwProvider::GetProviderEvents() const {
	std::vector<EVENT_DESCRIPTOR> events;
	if (m_EventCount == 0)
		return m_Events;

	if (m_Events.empty()) {
		ULONG size = 0;
		auto error = ::TdhEnumerateManifestProviderEvents((LPGUID)&m_Guid, nullptr, &size);
		if (error != ERROR_INSUFFICIENT_BUFFER)
			return m_Events;

		auto buffer = std::make_unique<BYTE[]>(size);
		if (!buffer)
			return m_Events;

		auto data = reinterpret_cast<PROVIDER_EVENT_INFO*>(buffer.get());
		error = ::TdhEnumerateManifestProviderEvents((LPGUID)&m_Guid, data, &size);
		assert(error == ERROR_SUCCESS);
		if (error != ERROR_SUCCESS)
			return m_Events;

		events.reserve(data->NumberOfEvents);
		for (ULONG i = 0; i < data->NumberOfEvents; i++)
			events.push_back(data->EventDescriptorsArray[i]);
		m_Events = std::move(events);
		m_EventCount = static_cast<int32_t>(m_Events.size());
	}
	return m_Events;
}

EtwEventInfo const& EtwProvider::EventInfo(const EVENT_DESCRIPTOR& desc) const {
	static EtwEventInfo dummy;

	ULONG id = ((ULONG)desc.Id << 8) | desc.Version;
	if (auto it = m_EventInfo.find(id); it != m_EventInfo.end())
		return it->second;

	EtwEventInfo info;
	info.ProviderGuid = GUID_NULL;

	ULONG size = 0;
	::TdhGetManifestEventInformation((LPGUID)&m_Guid, (PEVENT_DESCRIPTOR)&desc, nullptr, &size);
	auto buffer = std::make_unique<BYTE[]>(size);
	if (!buffer)
		return dummy;

	auto data = reinterpret_cast<TRACE_EVENT_INFO*>(buffer.get());
	auto error = ::TdhGetManifestEventInformation((LPGUID)&m_Guid, (PEVENT_DESCRIPTOR)&desc, data, &size);
	assert(error == ERROR_SUCCESS);
	if (ERROR_SUCCESS != error)
		return dummy;

	info.ProviderGuid = data->ProviderGuid;
	info.EventGuid = data->EventGuid;
	info.Descriptor = desc;
	info.DescodingSource = static_cast<EtwDecodingSource>(data->DecodingSource);
	info.Tags = data->Tags;

	if (data->EventNameOffset)
		info.EventName = (PCWSTR)(buffer.get() + data->EventNameOffset);
	if (data->EventAttributesOffset)
		info.EventAttributes = (PCWSTR)(buffer.get() + data->EventAttributesOffset);
	if (data->ChannelNameOffset)
		info.ChannelName = (PCWSTR)(buffer.get() + data->ChannelNameOffset);
	if (data->KeywordsNameOffset)
		info.KeywordName = (PCWSTR)(buffer.get() + data->KeywordsNameOffset);
	if (data->LevelNameOffset)
		info.LevelName = (PCWSTR)(buffer.get() + data->LevelNameOffset);
	else
		info.LevelName = L"Log Always";
	if (data->OpcodeNameOffset)
		info.OpCodeName = (PCWSTR)(buffer.get() + data->OpcodeNameOffset);
	if (data->EventMessageOffset)
		info.EventMessage = (PCWSTR)(buffer.get() + data->EventMessageOffset);
	if (data->TaskNameOffset)
		info.TaskName = (PCWSTR)(buffer.get() + data->TaskNameOffset);
	if (data->ProviderMessageOffset)
		info.ProviderMessage = (PCWSTR)(buffer.get() + data->ProviderMessageOffset);
	if (data->BinaryXMLOffset) {
		info.BinaryXML.resize(data->BinaryXMLSize);
		memcpy(info.BinaryXML.data(), buffer.get() + data->BinaryXMLOffset, data->BinaryXMLSize);
	}
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

	m_EventInfo.insert({ id, std::move(info) });
	return m_EventInfo[id];
}

int32_t EtwProvider::EventCount() const {
	if (m_EventCount >= 0)
		return m_EventCount;

	if (m_Source == EtwSchemaSource::Mof)
		return MofEventCount();

	ULONG size = 0;
	auto error = ::TdhEnumerateManifestProviderEvents((LPGUID)&m_Guid, nullptr, &size);
	if (error != ERROR_INSUFFICIENT_BUFFER)
		return 0;

	auto buffer = std::make_unique<BYTE[]>(size);
	if (!buffer)
		return 0;

	auto data = reinterpret_cast<PROVIDER_EVENT_INFO*>(buffer.get());
	error = ::TdhEnumerateManifestProviderEvents((LPGUID)&m_Guid, data, &size);
	if (error != ERROR_SUCCESS)
		return 0;
	return m_EventCount = data->NumberOfEvents;
}

std::vector<EtwFieldInfo> EtwProvider::FieldInfo(EtwFieldType type) const {
	std::vector<EtwFieldInfo> fields;

	ULONG size = 0;
	if (ERROR_INSUFFICIENT_BUFFER !=
		::TdhEnumerateProviderFieldInformation((LPGUID)&m_Guid, (EVENT_FIELD_TYPE)type, nullptr, &size))
		return fields;

	auto buffer = std::make_unique<BYTE[]>(size);
	auto info = reinterpret_cast<PPROVIDER_FIELD_INFOARRAY>(buffer.get());
	if (ERROR_SUCCESS != ::TdhEnumerateProviderFieldInformation((LPGUID)&m_Guid, (EVENT_FIELD_TYPE)type, info, &size))
		return fields;

	fields.reserve(info->NumberOfElements);
	for (ULONG i = 0; i < info->NumberOfElements; i++) {
		auto& item = info->FieldInfoArray[i];
		EtwFieldInfo info;
		info.Value = item.Value;
		if (item.NameOffset)
			info.Name = (PCWSTR)(buffer.get() + item.NameOffset);
		if (item.DescriptionOffset)
			info.Desc = (PCWSTR)(buffer.get() + item.DescriptionOffset);
		fields.push_back(std::move(info));
	}
	return fields;
}

int32_t EtwProvider::MofEventCount() const {
	wil::com_ptr<IWbemServices> spWmi;
	WMIHelper::Init(nullptr, L"root\\WMI", spWmi.addressof());
	if (!spWmi)
		return 0;

	wil::com_ptr<IEnumWbemClassObject> spEnum;
	spWmi->ExecQuery(CComBSTR(L"WQL"), CComBSTR(L"SELECT * FROM meta_class WHERE __superclass = 'EventTrace'"),
		WBEM_FLAG_FORWARD_ONLY, nullptr, spEnum.addressof());
	if (spEnum) {
		wil::com_ptr<IWbemClassObject> spClass;
		ULONG ret;
		while (S_OK == spEnum->Next(WBEM_INFINITE, 1, spClass.addressof(), &ret)) {
			wil::com_ptr<IWbemQualifierSet> spQualifiers;
			spClass->GetQualifierSet(spQualifiers.addressof());
			if (spQualifiers) {
				CComVariant value;
				if (S_OK == spQualifiers->Get(L"guid", 0, &value, nullptr) && _wcsicmp(value.bstrVal, m_GuidString.c_str()) == 0) {
					auto className = WMIHelper::GetStringProperty(spClass.get(), L"__CLASS");
					spEnum.reset();
					spWmi->ExecQuery(CComBSTR(L"WQL"), CComBSTR((L"SELECT * FROM meta_class WHERE __superclass = '" + className + L"'").c_str()),
						WBEM_FLAG_FORWARD_ONLY, nullptr, spEnum.addressof());
					if (spEnum) {
						wil::com_ptr<IWbemClassObject> spObject;
						while (S_OK == spEnum->Next(WBEM_INFINITE, 1, spObject.addressof(), &ret)) {
							wil::com_ptr<IWbemQualifierSet> spQualifiers;
							spObject->GetQualifierSet(spQualifiers.addressof());
							if (spQualifiers) {
								CComVariant value;
								auto names = WMIHelper::GetNames(spQualifiers.get());
								if (S_OK == spQualifiers->Get(L"displayname", 0, &value, nullptr)) {

								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}
