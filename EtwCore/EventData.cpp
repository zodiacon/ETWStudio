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

EventData::EventData(PEVENT_RECORD rec, std::wstring processName, const std::wstring& eventName, uint32_t index) : m_Record(rec),
	m_ProcessName(std::move(processName)), m_EventName(std::move(eventName)), m_Index(index) {
	auto& header = rec->EventHeader;
	m_HeaderFlags = header.Flags;
	m_ProcessId = header.ProcessId;
	m_ThreadId = header.ThreadId;
	m_ProviderId = header.ProviderId;
	_timeStamp = header.TimeStamp.QuadPart;
	m_EventDescriptor = header.EventDescriptor;

	// parse event specific data

	ULONG size = 0;
	auto error = ::TdhGetEventInformation(rec, 0, nullptr, nullptr, &size);
	if (error == ERROR_INSUFFICIENT_BUFFER) {
		m_Buffer = std::make_unique<BYTE[]>(size);
		auto info = reinterpret_cast<PTRACE_EVENT_INFO>(m_Buffer.get());
		error = ::TdhGetEventInformation(rec, 0, nullptr, info, &size);
	}
	::SetLastError(error);
}

void* EventData::operator new(size_t size) {
	if (InterlockedIncrement(&s_Count) == 1) {
		InitializeCriticalSection(&s_HeapLock);
		s_hHeap = ::HeapCreate(HEAP_NO_SERIALIZE, 1 << 24, 0);
		assert(s_hHeap);
	}

	EnterCriticalSection(&s_HeapLock);

	const LPVOID p = ::HeapAlloc(s_hHeap, 0, size);

	LeaveCriticalSection(&s_HeapLock);

	return p;
}

void EventData::operator delete(void* p) {

	EnterCriticalSection(&s_HeapLock);

	::HeapFree(s_hHeap, 0, p);

	LeaveCriticalSection(&s_HeapLock);

	if (InterlockedDecrement(&s_Count) == 0) {
		if (s_hHeap) {
			::HeapDestroy(s_hHeap);
		}
	}
}

DWORD EventData::GetProcessId() const {
	return m_ProcessId;
}

const std::wstring& EventData::GetProcessName() const {
	return m_ProcessName;
}

const std::wstring& EventData::GetEventName() const {
	return m_EventName;
}

DWORD EventData::GetThreadId() const {
	return m_ThreadId;
}

ULONGLONG EventData::GetTimeStamp() const {
	return _timeStamp;
}

const GUID& EventData::GetProviderId() const {
	return m_ProviderId;
}

const EVENT_DESCRIPTOR& EventData::GetEventDescriptor() const {
	return m_EventDescriptor;
}

const std::vector<EventProperty>& EventData::GetProperties() const {
	if (!m_Properties.empty() || m_Buffer == nullptr)
		return m_Properties;

	auto info = reinterpret_cast<PTRACE_EVENT_INFO>(m_Buffer.get());
	m_Properties.reserve(info->TopLevelPropertyCount);
	auto userDataLength = m_Record->UserDataLength;
	auto data = (BYTE*)m_Record->UserData;

	for (ULONG i = 0; i < info->TopLevelPropertyCount && userDataLength > 0; i++) {
		auto& prop = info->EventPropertyInfoArray[i];
		EventProperty property(prop);
		property.Name.assign((WCHAR*)((BYTE*)info + prop.NameOffset));
		ULONG len = prop.length;
		if (len == 0) {
			PROPERTY_DATA_DESCRIPTOR desc;
			desc.PropertyName = (ULONGLONG)property.Name.c_str();
			desc.ArrayIndex = ULONG_MAX;
			::TdhGetPropertySize(m_Record, 0, nullptr, 1, &desc, &len);
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
	if (m_Properties.empty()) {
		m_Buffer.release();
	}
	return m_Properties;
}

const EventProperty* EventData::GetProperty(PCWSTR name) const {
	for (auto& prop : GetProperties())
		if (prop.Name == name)
			return &prop;
	return nullptr;
}

uint32_t EventData::GetIndex() const {
	return m_Index;
}

std::wstring EventData::FormatProperty(const EventProperty& property) const {
	ULONG size = 0;
	PEVENT_MAP_INFO eventMap = nullptr;
	std::unique_ptr<BYTE[]> mapBuffer;
	auto& prop = property.Info;
	WCHAR buffer[1024];
	auto info = reinterpret_cast<PTRACE_EVENT_INFO>(m_Buffer.get());
	if (prop.nonStructType.MapNameOffset) {
		auto mapName = (PWSTR)((PBYTE)info + prop.nonStructType.MapNameOffset);
		if(ERROR_INSUFFICIENT_BUFFER == ::TdhGetEventMapInformation(m_Record, mapName, nullptr, &size)) {
			mapBuffer = std::make_unique<BYTE[]>(size);
			eventMap = reinterpret_cast<PEVENT_MAP_INFO>(mapBuffer.get());
			if(ERROR_SUCCESS != ::TdhGetEventMapInformation(m_Record, mapName, eventMap, &size))
				eventMap = nullptr;
		}
	}
	size = sizeof(buffer);

	// length special case for IPV6
	auto len = prop.length;
	if (prop.nonStructType.InType == TDH_INTYPE_BINARY && prop.nonStructType.OutType == TDH_OUTTYPE_IPV6)
		len = sizeof(IN6_ADDR);

	USHORT consumed;
	auto status = ::TdhFormatProperty(info, eventMap, (m_HeaderFlags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? 4 : 8,
		prop.nonStructType.InType, prop.nonStructType.OutType, len, (USHORT)property.GetLength(), (PBYTE)property.GetData(),
		&size, buffer, &consumed);
	if (status == ERROR_SUCCESS)
		return std::wstring(buffer);

	return L"";
}

uint64_t EventData::GetEventKey() const {
	return m_ProviderId.Data1 ^ m_EventDescriptor.Opcode;
}

void EventData::SetProcessName(std::wstring name) {
	m_ProcessName = std::move(name);
}

