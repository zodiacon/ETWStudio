#include "pch.h"
#include "EtwProvider.h"
#include <memory>
#include <algorithm>
#include <assert.h>
#include "WMIHelper.h"
#include <wil\com.h>

#pragma comment(lib, "tdh")

//
// m_EventInfo key. Manifest events are identified by (id, version), but classic (MOF)
// events always have id 0 and are identified by (opcode, version) instead.
//
static ULONG ManifestEventKey(EVENT_DESCRIPTOR const& desc) {
	return ((ULONG)desc.Id << 8) | desc.Version;
}

static ULONG MofEventKey(EVENT_DESCRIPTOR const& desc) {
	return ((ULONG)desc.Opcode << 8) | desc.Version;
}

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
	if (m_Source == EtwSchemaSource::Mof) {
		BuildMofSchema();
		return m_Events;
	}

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

	if (m_Source == EtwSchemaSource::Mof) {
		// the whole MOF schema is built in one WMI walk, so it is either cached or absent
		BuildMofSchema();
		if (auto it = m_EventInfo.find(MofEventKey(desc)); it != m_EventInfo.end())
			return it->second;
		return dummy;
	}

	ULONG id = ManifestEventKey(desc);
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
		info.Properties.push_back(std::move(prop));
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
	BuildMofSchema();
	return m_EventCount < 0 ? 0 : m_EventCount;
}

//
// MOF schema walk.
//
// The ETW MOF classes in root\WMI form a three-level hierarchy below EventTrace:
//
//   EventTrace
//    +- Process                     Guid("{3d6fa8d0-...}")            provider class
//        +- Process_V2              EventVersion(2)                   version class
//            +- Process_V2_TypeGroup1
//                                   EventType({1,2,3,4})
//                                   EventTypeName({"Start","End",...})
//                                   [WmiDataId(1)] uint32 ProcessId; ...
//
// Providers that never versioned their events hang the type groups directly off the
// provider class, so a type group is recognized by carrying an EventType qualifier
// rather than by its depth.
//
namespace {
	struct MofClass {
		wil::com_ptr<IWbemClassObject> Object;
		std::wstring Name;
	};

	struct MofEvent {
		UCHAR Type;
		UCHAR Version;
		std::wstring Name;
		std::vector<EtwEventProperty> Properties;
	};

	bool HasQualifier(IWbemQualifierSet* qualifiers, PCWSTR name) {
		if (qualifiers == nullptr)
			return false;
		CComVariant value;
		return S_OK == qualifiers->Get(name, 0, &value, nullptr);
	}

	std::wstring GetStringQualifier(IWbemQualifierSet* qualifiers, PCWSTR name) {
		if (qualifiers == nullptr)
			return L"";
		CComVariant value;
		if (S_OK != qualifiers->Get(name, 0, &value, nullptr) || value.vt != VT_BSTR)
			return L"";
		return value.bstrVal;
	}

	int32_t GetIntQualifier(IWbemQualifierSet* qualifiers, PCWSTR name, int32_t defaultValue) {
		if (qualifiers == nullptr)
			return defaultValue;
		CComVariant value;
		if (S_OK != qualifiers->Get(name, 0, &value, nullptr))
			return defaultValue;
		if (value.vt == VT_I4)
			return value.lVal;
		if (SUCCEEDED(value.ChangeType(VT_I4)))
			return value.lVal;
		return defaultValue;
	}

	//
	// EventType/EventTypeName are single valued when the group covers one event and
	// SAFEARRAYs (parallel to each other) when it covers several.
	//
	std::vector<int32_t> GetInt32Qualifiers(IWbemQualifierSet* qualifiers, PCWSTR name) {
		std::vector<int32_t> values;
		if (qualifiers == nullptr)
			return values;

		CComVariant value;
		if (S_OK != qualifiers->Get(name, 0, &value, nullptr))
			return values;

		if (value.vt == VT_I4) {
			values.push_back(value.lVal);
		}
		else if (value.vt == (VT_ARRAY | VT_I4)) {
			LONG lower, upper;
			if (SUCCEEDED(::SafeArrayGetLBound(value.parray, 1, &lower)) &&
				SUCCEEDED(::SafeArrayGetUBound(value.parray, 1, &upper))) {
				for (LONG i = lower; i <= upper; i++) {
					LONG item;
					if (SUCCEEDED(::SafeArrayGetElement(value.parray, &i, &item)))
						values.push_back(item);
				}
			}
		}
		return values;
	}

	std::vector<std::wstring> GetStringQualifiers(IWbemQualifierSet* qualifiers, PCWSTR name) {
		std::vector<std::wstring> values;
		if (qualifiers == nullptr)
			return values;

		CComVariant value;
		if (S_OK != qualifiers->Get(name, 0, &value, nullptr))
			return values;

		if (value.vt == VT_BSTR) {
			values.push_back(value.bstrVal);
		}
		else if (value.vt == (VT_ARRAY | VT_BSTR)) {
			LONG lower, upper;
			if (SUCCEEDED(::SafeArrayGetLBound(value.parray, 1, &lower)) &&
				SUCCEEDED(::SafeArrayGetUBound(value.parray, 1, &upper))) {
				for (LONG i = lower; i <= upper; i++) {
					BSTR item = nullptr;
					if (SUCCEEDED(::SafeArrayGetElement(value.parray, &i, &item))) {
						CComBSTR owned;
						owned.Attach(item);
						values.push_back(owned.m_str ? owned.m_str : L"");
					}
				}
			}
		}
		return values;
	}

	std::vector<MofClass> EnumSubClasses(IWbemServices* wmi, PCWSTR superClass) {
		std::vector<MofClass> classes;

		auto query = L"SELECT * FROM meta_class WHERE __superclass = '" + std::wstring(superClass) + L"'";
		wil::com_ptr<IEnumWbemClassObject> spEnum;
		if (FAILED(wmi->ExecQuery(CComBSTR(L"WQL"), CComBSTR(query.c_str()),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, spEnum.put())) || !spEnum)
			return classes;

		for (;;) {
			// a fresh com_ptr per iteration - put() releases, addressof() would leak
			wil::com_ptr<IWbemClassObject> spClass;
			ULONG returned = 0;
			if (S_OK != spEnum->Next(WBEM_INFINITE, 1, spClass.put(), &returned) || returned == 0)
				break;

			MofClass mc;
			mc.Name = WMIHelper::GetStringProperty(spClass.get(), L"__CLASS");
			mc.Object = std::move(spClass);
			classes.push_back(std::move(mc));
		}
		return classes;
	}

	void MapMofPropertyType(CIMTYPE cimType, IWbemQualifierSet* qualifiers, EtwEventProperty& prop) {
		USHORT inType = TDH_INTYPE_BINARY, outType = TDH_OUTTYPE_NULL;

		switch (cimType & ~CIM_FLAG_ARRAY) {
			case CIM_SINT8:   inType = TDH_INTYPE_INT8; break;
			case CIM_UINT8:   inType = TDH_INTYPE_UINT8; break;
			case CIM_SINT16:  inType = TDH_INTYPE_INT16; break;
			case CIM_UINT16:  inType = TDH_INTYPE_UINT16; break;
			case CIM_SINT32:  inType = TDH_INTYPE_INT32; break;
			case CIM_UINT32:  inType = TDH_INTYPE_UINT32; break;
			case CIM_SINT64:  inType = TDH_INTYPE_INT64; break;
			case CIM_UINT64:  inType = TDH_INTYPE_UINT64; break;
			case CIM_REAL32:  inType = TDH_INTYPE_FLOAT; break;
			case CIM_REAL64:  inType = TDH_INTYPE_DOUBLE; break;
			case CIM_BOOLEAN: inType = TDH_INTYPE_BOOLEAN; break;
			case CIM_CHAR16:  inType = TDH_INTYPE_UNICODECHAR; break;
			case CIM_DATETIME: inType = TDH_INTYPE_FILETIME; break;
			case CIM_STRING:
			{
				auto wide = _wcsicmp(GetStringQualifier(qualifiers, L"format").c_str(), L"w") == 0;
				auto termination = GetStringQualifier(qualifiers, L"StringTermination");
				if (_wcsicmp(termination.c_str(), L"Counted") == 0)
					inType = wide ? TDH_INTYPE_COUNTEDSTRING : TDH_INTYPE_COUNTEDANSISTRING;
				else if (_wcsicmp(termination.c_str(), L"ReverseCounted") == 0)
					inType = wide ? TDH_INTYPE_REVERSEDCOUNTEDSTRING : TDH_INTYPE_REVERSEDCOUNTEDANSISTRING;
				else if (_wcsicmp(termination.c_str(), L"NotCounted") == 0)
					inType = wide ? TDH_INTYPE_NONNULLTERMINATEDSTRING : TDH_INTYPE_NONNULLTERMINATEDANSISTRING;
				else
					inType = wide ? TDH_INTYPE_UNICODESTRING : TDH_INTYPE_ANSISTRING;
				break;
			}
			default: inType = TDH_INTYPE_BINARY; break;
		}

		// a "pointer" property is trace-pointer-sized, not host-pointer-sized
		if (HasQualifier(qualifiers, L"pointer") || HasQualifier(qualifiers, L"PointerType"))
			inType = TDH_INTYPE_POINTER;

		// the "extension" qualifier is what MOF uses to say "this is really a GUID/SID/IP/..."
		auto extension = GetStringQualifier(qualifiers, L"extension");
		if (!extension.empty()) {
			if (_wcsicmp(extension.c_str(), L"Guid") == 0)
				inType = TDH_INTYPE_GUID;
			else if (_wcsicmp(extension.c_str(), L"Sid") == 0)
				inType = TDH_INTYPE_WBEMSID;
			else if (_wcsicmp(extension.c_str(), L"SizeT") == 0)
				inType = TDH_INTYPE_SIZET;
			else if (_wcsicmp(extension.c_str(), L"Variant") == 0)
				inType = TDH_INTYPE_BINARY;
			else if (_wcsicmp(extension.c_str(), L"Port") == 0)
				outType = TDH_OUTTYPE_PORT;
			else if (_wcsicmp(extension.c_str(), L"IPAddr") == 0 || _wcsicmp(extension.c_str(), L"IPAddrV4") == 0)
				outType = TDH_OUTTYPE_IPV4;
			else if (_wcsicmp(extension.c_str(), L"IPAddrV6") == 0) {
				inType = TDH_INTYPE_BINARY;
				outType = TDH_OUTTYPE_IPV6;
			}
			else if (_wcsicmp(extension.c_str(), L"WmiTime") == 0)
				outType = TDH_OUTTYPE_DATETIME;
			else if (_wcsicmp(extension.c_str(), L"NoPrint") == 0)
				outType = TDH_OUTTYPE_NOPRINT;
		}

		if (outType == TDH_OUTTYPE_NULL && _wcsicmp(GetStringQualifier(qualifiers, L"format").c_str(), L"x") == 0)
			outType = (inType == TDH_INTYPE_UINT64 || inType == TDH_INTYPE_INT64) ? TDH_OUTTYPE_HEXINT64 : TDH_OUTTYPE_HEXINT32;

		prop.InType = inType;
		prop.OutType = outType;
	}

	std::vector<EtwEventProperty> GetMofProperties(IWbemClassObject* obj) {
		struct OrderedProperty {
			int32_t DataId;
			EtwEventProperty Prop;
		};
		std::vector<OrderedProperty> ordered;

		if (FAILED(obj->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY)))
			return {};

		for (;;) {
			CComBSTR name;
			CComVariant value;
			CIMTYPE type = 0;
			LONG flavor = 0;
			if (S_OK != obj->Next(0, &name, &value, &type, &flavor))
				break;

			wil::com_ptr<IWbemQualifierSet> spQualifiers;
			obj->GetPropertyQualifierSet(name, spQualifiers.put());

			// only properties carrying WmiDataId are part of the on-the-wire payload;
			// the qualifier is also what gives their order, which is not the enumeration order
			auto dataId = GetIntQualifier(spQualifiers.get(), L"WmiDataId", 0);
			if (dataId <= 0)
				continue;

			EtwEventProperty prop;
			prop.Name = name.m_str ? name.m_str : L"";
			prop.Flags = EtwPropertyFlags::None;
			MapMofPropertyType(type, spQualifiers.get(), prop);
			ordered.push_back({ dataId, std::move(prop) });
		}
		obj->EndEnumeration();

		std::sort(ordered.begin(), ordered.end(), [](auto const& p1, auto const& p2) {
			return p1.DataId < p2.DataId;
			});

		std::vector<EtwEventProperty> props;
		props.reserve(ordered.size());
		for (auto& o : ordered)
			props.push_back(std::move(o.Prop));
		return props;
	}

	std::vector<MofEvent> HarvestTypeGroup(MofClass const& group, UCHAR defaultVersion) {
		std::vector<MofEvent> events;

		wil::com_ptr<IWbemQualifierSet> spQualifiers;
		group.Object->GetQualifierSet(spQualifiers.put());

		auto types = GetInt32Qualifiers(spQualifiers.get(), L"EventType");
		if (types.empty())
			return events;

		auto names = GetStringQualifiers(spQualifiers.get(), L"EventTypeName");
		auto version = static_cast<UCHAR>(GetIntQualifier(spQualifiers.get(), L"EventVersion", defaultVersion));
		auto properties = GetMofProperties(group.Object.get());

		events.reserve(types.size());
		for (size_t i = 0; i < types.size(); i++) {
			MofEvent evt;
			evt.Type = static_cast<UCHAR>(types[i]);
			evt.Version = version;
			// EventTypeName is parallel to EventType, but is allowed to be absent or short
			evt.Name = i < names.size() ? names[i] : group.Name;
			evt.Properties = properties;
			events.push_back(std::move(evt));
		}
		return events;
	}
}

bool EtwProvider::BuildMofSchema() const {
	if (m_MofSchemaBuilt)
		return !m_Events.empty();

	m_MofSchemaBuilt = true;
	m_EventCount = 0;

	wil::com_ptr<IWbemServices> spWmi;
	WMIHelper::Init(nullptr, L"root\\WMI", spWmi.put());
	if (!spWmi)
		return false;

	//
	// the provider class is the direct subclass of EventTrace whose Guid qualifier
	// matches us. StringFromGUID2 and the MOF qualifier both use the braced form.
	//
	std::wstring providerClass;
	for (auto& mc : EnumSubClasses(spWmi.get(), L"EventTrace")) {
		wil::com_ptr<IWbemQualifierSet> spQualifiers;
		mc.Object->GetQualifierSet(spQualifiers.put());
		if (_wcsicmp(GetStringQualifier(spQualifiers.get(), L"Guid").c_str(), m_GuidString.c_str()) == 0) {
			providerClass = mc.Name;
			break;
		}
	}
	if (providerClass.empty())
		return false;

	std::vector<MofEvent> events;
	for (auto& sub : EnumSubClasses(spWmi.get(), providerClass.c_str())) {
		wil::com_ptr<IWbemQualifierSet> spQualifiers;
		sub.Object->GetQualifierSet(spQualifiers.put());

		if (HasQualifier(spQualifiers.get(), L"EventType")) {
			// unversioned provider: type groups sit directly under the provider class
			auto harvested = HarvestTypeGroup(sub, 0);
			events.insert(events.end(), harvested.begin(), harvested.end());
		}
		else {
			auto version = static_cast<UCHAR>(GetIntQualifier(spQualifiers.get(), L"EventVersion", 0));
			for (auto& group : EnumSubClasses(spWmi.get(), sub.Name.c_str())) {
				auto harvested = HarvestTypeGroup(group, version);
				events.insert(events.end(), harvested.begin(), harvested.end());
			}
		}
	}

	m_Events.reserve(events.size());
	for (auto& evt : events) {
		EVENT_DESCRIPTOR desc{};
		// classic events have no id - opcode is the discriminator
		desc.Opcode = evt.Type;
		desc.Version = evt.Version;

		EtwEventInfo info;
		info.ProviderGuid = m_Guid;
		info.EventGuid = m_Guid;
		info.Descriptor = desc;
		info.DescodingSource = EtwDecodingSource::Wbem;
		info.ProviderName = m_name;
		info.TaskName = providerClass;
		info.EventName = evt.Name;
		info.OpCodeName = evt.Name;
		info.LevelName = L"Log Always";
		info.Tags = 0;
		info.Properties = std::move(evt.Properties);

		m_EventInfo.insert({ MofEventKey(desc), std::move(info) });
		m_Events.push_back(desc);
	}
	m_EventCount = static_cast<int32_t>(m_Events.size());
	return !m_Events.empty();
}
