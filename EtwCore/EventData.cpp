#include "pch.h"
#include "EventData.h"
#include <in6addr.h>

// EventProperty

EventProperty::EventProperty(EVENT_PROPERTY_INFO& info) : Info(info) {
}

void* EventProperty::Allocate(ULONG size) {
	Data.resize(size);
	return Data.data();
}

PCWSTR EventProperty::GetUnicodeString() const {
	return (PCWSTR)Data.data();
}

PCSTR EventProperty::GetAnsiString() const {
	return (PCSTR)Data.data();
}

// EventData

EventData::EventData(PEVENT_RECORD rec, std::wstring processName, uint32_t index) : 
	m_Record(*rec), m_Header(rec->EventHeader), m_ProcessName(std::move(processName)), m_Index(index) {
	//
	// parse event specific data
	//
	ULONG size = 0;
	auto error = ::TdhGetEventInformation(rec, 0, nullptr, nullptr, &size);
	if (error == ERROR_INSUFFICIENT_BUFFER) {
		auto buffer = ::HeapAlloc(s_hHeap2, 0, size);
		auto info = reinterpret_cast<PTRACE_EVENT_INFO>(buffer);
		error = ::TdhGetEventInformation(rec, 0, nullptr, info, &size);
		m_EventInfo = info;
	}
	if (error == ERROR_SUCCESS)
		GetProperties();
	::SetLastError(error);
}

EventData::~EventData() {
	if (m_EventInfo)
		::HeapFree(s_hHeap2, HEAP_NO_SERIALIZE, m_EventInfo);
}

void* EventData::operator new(size_t size) {
	if (InterlockedIncrement(&s_Count) == 1) {
		InitializeCriticalSection(&s_HeapLock);
		s_hHeap = ::HeapCreate(0, 1 << 24, 0);
		s_hHeap2 = ::HeapCreate(0, 1 << 24, 0);
		assert(s_hHeap && s_hHeap2);
	}

	return ::HeapAlloc(s_hHeap, 0, size);
}

void EventData::operator delete(void* p) {
	::HeapFree(s_hHeap, 0, p);

	if (InterlockedDecrement(&s_Count) == 0) {
		if (s_hHeap) {
			::HeapDestroy(s_hHeap);
			::HeapDestroy(s_hHeap2);
			s_hHeap = s_hHeap2 = nullptr;
		}
	}
}

DWORD EventData::GetProcessId() const {
	return m_ProcessId ? m_ProcessId : m_Header.ProcessId;
}

const std::wstring& EventData::GetProcessName() const {
	return m_ProcessName;
}

DWORD EventData::GetThreadId() const {
	return m_ThreadId ? m_ThreadId : m_Header.ThreadId;
}

EVENT_HEADER const& EventData::GetEventHeader() const {
	return m_Header;
}

ULONGLONG EventData::GetTimeStamp() const noexcept {
	return m_Header.TimeStamp.QuadPart;
}

const GUID& EventData::GetProviderId() const noexcept {
	return m_Header.ProviderId;
}

const EVENT_DESCRIPTOR& EventData::GetEventDescriptor() const {
	return m_Header.EventDescriptor;
}

const std::vector<EventProperty>& EventData::GetProperties() const {
	if (!m_Properties.empty() || m_EventInfo == nullptr)
		return m_Properties;

	auto info = m_EventInfo;
	m_Properties.reserve(info->TopLevelPropertyCount);
	auto userDataLength = m_Record.UserDataLength;
	auto data = (BYTE*)m_Record.UserData;

	for (ULONG i = 0; i < info->TopLevelPropertyCount && userDataLength > 0; i++) {
		auto& prop = info->EventPropertyInfoArray[i];
		EventProperty property(prop);
		property.Name.assign((WCHAR*)((BYTE*)info + prop.NameOffset));
		ULONG len = prop.length;
		if (prop.Flags & PropertyParamLength) {
			len = m_Properties[len].GetValue<int16_t>();
		}
		if (len == 0) {
			PROPERTY_DATA_DESCRIPTOR desc;
			desc.PropertyName = (ULONGLONG)property.Name.c_str();
			desc.ArrayIndex = ULONG_MAX;
			::TdhGetPropertySize((PEVENT_RECORD)&m_Record, 0, nullptr, 1, &desc, &len);
		}
		if (len) {
			auto d = property.Allocate(len);
			if (d) {
				::memcpy(d, data, len);
			}
			data += len;
			if (userDataLength < len)
				break;
			userDataLength -= (USHORT)len;
		}
		m_Properties.push_back(std::move(property));
	}

	return m_Properties;
}

const EventProperty* EventData::GetProperty(PCWSTR name) const noexcept {
	for (auto& prop : GetProperties())
		if (prop.Name == name)
			return &prop;
	return nullptr;
}

uint32_t EventData::GetIndex() const noexcept {
	return m_Index;
}

std::wstring EventData::FormatProperty(const EventProperty& property) const {
	ULONG size = 0;
	PEVENT_MAP_INFO eventMap = nullptr;
	std::unique_ptr<BYTE[]> mapBuffer;
	auto& prop = property.Info;
	WCHAR buffer[1024];
	auto info = m_EventInfo;
	if (prop.nonStructType.MapNameOffset) {
		auto mapName = (PWSTR)((PBYTE)info + prop.nonStructType.MapNameOffset);
		if (ERROR_INSUFFICIENT_BUFFER == ::TdhGetEventMapInformation((PEVENT_RECORD)&m_Record, mapName, nullptr, &size)) {
			mapBuffer = std::make_unique<BYTE[]>(size);
			eventMap = reinterpret_cast<PEVENT_MAP_INFO>(mapBuffer.get());
			if (ERROR_SUCCESS != ::TdhGetEventMapInformation((PEVENT_RECORD)&m_Record, mapName, eventMap, &size))
				eventMap = nullptr;
		}
	}
	size = sizeof(buffer);

	// length special case for IPV6
	auto len = prop.length;
	if (prop.nonStructType.InType == TDH_INTYPE_BINARY && prop.nonStructType.OutType == TDH_OUTTYPE_IPV6)
		len = sizeof(IN6_ADDR);

	USHORT consumed;
	auto status = ::TdhFormatProperty(info, eventMap, (m_Header.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? 4 : 8,
		prop.nonStructType.InType, prop.nonStructType.OutType, len, (USHORT)property.GetLength(), (PBYTE)property.GetData(),
		&size, buffer, &consumed);
	if (status == ERROR_SUCCESS)
		return std::wstring(buffer);

	return L"";
}

void EventData::SetProcessName(std::wstring name) {
	m_ProcessName = std::move(name);
}

EventStrings const& EventData::GetEventStrings() const {
	if (!m_Strings._HasValue) {
		auto info = m_EventInfo;
		if (info) {
			EventStrings str;
			if (info->EventNameOffset) {
				m_Strings.Name = (PCWSTR)((PBYTE)info + info->EventNameOffset);
			}
			if (info->TaskNameOffset) {
				m_Strings.Task = (PCWSTR)((PBYTE)info + info->TaskNameOffset);
			}
			if (info->ChannelNameOffset) {
				m_Strings.Channel = (PCWSTR)((PBYTE)info + info->ChannelNameOffset);
			}
			if (info->KeywordsNameOffset) {
				m_Strings.Keyword = (PCWSTR)((PBYTE)info + info->KeywordsNameOffset);
			}
			if (info->OpcodeNameOffset) {
				m_Strings.Opcode = (PCWSTR)((PBYTE)info + info->OpcodeNameOffset);
			}
			if (info->LevelNameOffset) {
				m_Strings.Level = (PCWSTR)((PBYTE)info + info->LevelNameOffset);
			}
			if (info->EventMessageOffset) {
				m_Strings.Message = (PCWSTR)((PBYTE)info + info->EventMessageOffset);
			}
			if (info->ProviderMessageOffset) {
				m_Strings.ProviderMessage = (PCWSTR)((PBYTE)info + info->ProviderMessageOffset);
			}
			if (info->EventAttributesOffset) {
				m_Strings.EventAttributes = (PCWSTR)((PBYTE)info + info->EventAttributesOffset);
			}
			if (info->RelatedActivityIDNameOffset) {
				m_Strings.RelatedActivity = (PCWSTR)((PBYTE)info + info->RelatedActivityIDNameOffset);
			}
		}
		m_Strings._HasValue = true;
	}
	return m_Strings;
}

int EventData::GetCPU() const noexcept {
	return GetEventProcessorIndex(&m_Record);
}
