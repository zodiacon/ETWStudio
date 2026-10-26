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

namespace {
	struct EventInfoKey {
		GUID Provider;
		EVENT_DESCRIPTOR Descriptor;
		USHORT Flags;

		bool operator==(EventInfoKey const& other) const noexcept {
			return Provider == other.Provider && Flags == other.Flags &&
				::memcmp(&Descriptor, &other.Descriptor, sizeof(Descriptor)) == 0;
		}
	};

	struct EventInfoKeyHash {
		size_t operator()(EventInfoKey const& key) const noexcept {
			auto p = reinterpret_cast<const uint64_t*>(&key.Provider);
			uint64_t d;
			static_assert(sizeof(d) <= sizeof(key.Descriptor));
			::memcpy(&d, &key.Descriptor, sizeof(d));
			return std::hash<uint64_t>()(p[0] ^ (p[1] * 31) ^ (d * 131) ^ key.Descriptor.Keyword ^ key.Flags);
		}
	};

	struct CachedEventInfo {
		std::unique_ptr<BYTE[]> Buffer;
		ULONG Error;
	};

	std::shared_mutex s_EventInfoLock;
	std::unordered_map<EventInfoKey, CachedEventInfo, EventInfoKeyHash> s_EventInfoCache;

	bool IsEventInfoCacheable(PEVENT_RECORD rec) {
		//
		// WPP events need TMF info, TraceLogging events carry their own schema per event
		//
		if (rec->EventHeader.Flags & EVENT_HEADER_FLAG_TRACE_MESSAGE)
			return false;
		for (USHORT i = 0; i < rec->ExtendedDataCount; i++)
			if (rec->ExtendedData[i].ExtType == EVENT_HEADER_EXT_TYPE_EVENT_SCHEMA_TL)
				return false;
		return true;
	}
}

EventData::EventData(PEVENT_RECORD rec, std::wstring processName, uint32_t index) : 
	m_Record(*rec), m_Header(rec->EventHeader), m_ProcessName(std::move(processName)), m_Index(index) {
	ULONG error = ERROR_SUCCESS;
	if (IsEventInfoCacheable(rec)) {
		m_EventInfo = GetCachedEventInfo(rec, error);
	}
	else {
		ULONG size = 0;
		error = ::TdhGetEventInformation(rec, 0, nullptr, nullptr, &size);
		if (error == ERROR_INSUFFICIENT_BUFFER) {
			auto info = reinterpret_cast<PTRACE_EVENT_INFO>(::HeapAlloc(s_hHeap, 0, size));
			error = ::TdhGetEventInformation(rec, 0, nullptr, info, &size);
			if (error == ERROR_SUCCESS) {
				m_EventInfo = info;
				m_OwnsEventInfo = true;
			}
			else {
				::HeapFree(s_hHeap, 0, info);
			}
		}
	}
	CopyRecordData(rec);
	//
	// properties are decoded lazily (GetProperties)
	//
	::SetLastError(error);
}

PTRACE_EVENT_INFO EventData::GetCachedEventInfo(PEVENT_RECORD rec, ULONG& error) {
	EventInfoKey key{ rec->EventHeader.ProviderId, rec->EventHeader.EventDescriptor,
		(USHORT)(rec->EventHeader.Flags & (EVENT_HEADER_FLAG_32_BIT_HEADER | EVENT_HEADER_FLAG_64_BIT_HEADER | EVENT_HEADER_FLAG_CLASSIC_HEADER)) };
	{
		std::shared_lock locker(s_EventInfoLock);
		if (auto it = s_EventInfoCache.find(key); it != s_EventInfoCache.end()) {
			error = it->second.Error;
			return reinterpret_cast<PTRACE_EVENT_INFO>(it->second.Buffer.get());
		}
	}

	CachedEventInfo cached;
	ULONG size = 0;
	error = ::TdhGetEventInformation(rec, 0, nullptr, nullptr, &size);
	if (error == ERROR_INSUFFICIENT_BUFFER) {
		cached.Buffer = std::make_unique<BYTE[]>(size);
		error = ::TdhGetEventInformation(rec, 0, nullptr, reinterpret_cast<PTRACE_EVENT_INFO>(cached.Buffer.get()), &size);
		if (error != ERROR_SUCCESS)
			cached.Buffer.reset();
	}
	cached.Error = error;

	std::lock_guard locker(s_EventInfoLock);
	auto [it, _] = s_EventInfoCache.try_emplace(key, std::move(cached));
	error = it->second.Error;
	return reinterpret_cast<PTRACE_EVENT_INFO>(it->second.Buffer.get());
}

void EventData::CopyRecordData(PEVENT_RECORD rec) {
	//
	// UserData and ExtendedData point into the ETW buffer, which is only valid during the callback.
	// Copy them into a single block (stack traces go to m_Stack instead).
	//
	auto align = [](size_t n) { return (n + 7) & ~size_t(7); };

	USHORT extCount = 0;
	size_t size = align(rec->UserDataLength);
	for (USHORT i = 0; i < rec->ExtendedDataCount; i++) {
		auto& item = rec->ExtendedData[i];
		if (item.ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE64) {
			auto count = (item.DataSize - sizeof(ULONG64)) / sizeof(ULONG64);
			auto stack = reinterpret_cast<PEVENT_EXTENDED_ITEM_STACK_TRACE64>(item.DataPtr);
			m_Stack.assign(stack->Address, stack->Address + count);
		}
		else if (item.ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE32) {
			auto count = (item.DataSize - sizeof(ULONG64)) / sizeof(ULONG);
			auto stack = reinterpret_cast<PEVENT_EXTENDED_ITEM_STACK_TRACE32>(item.DataPtr);
			m_Stack.assign(stack->Address, stack->Address + count);
		}
		else {
			extCount++;
			size += sizeof(EVENT_HEADER_EXTENDED_DATA_ITEM) + align(item.DataSize);
		}
	}

	m_Record.UserData = nullptr;
	m_Record.ExtendedData = nullptr;
	m_Record.ExtendedDataCount = 0;
	if (size == 0)
		return;

	m_RecordData = static_cast<BYTE*>(::HeapAlloc(s_hHeap, 0, size));
	if (!m_RecordData) {
		m_Record.UserDataLength = 0;
		return;
	}

	auto items = reinterpret_cast<PEVENT_HEADER_EXTENDED_DATA_ITEM>(m_RecordData);
	auto p = m_RecordData + extCount * sizeof(EVENT_HEADER_EXTENDED_DATA_ITEM);
	USHORT n = 0;
	for (USHORT i = 0; i < rec->ExtendedDataCount; i++) {
		auto& item = rec->ExtendedData[i];
		if (item.ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE64 || item.ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE32)
			continue;
		items[n] = item;
		::memcpy(p, (const void*)item.DataPtr, item.DataSize);
		items[n].DataPtr = (ULONGLONG)p;
		p += align(item.DataSize);
		n++;
	}
	if (extCount) {
		m_Record.ExtendedData = items;
		m_Record.ExtendedDataCount = extCount;
	}
	if (rec->UserDataLength) {
		::memcpy(p, rec->UserData, rec->UserDataLength);
		m_Record.UserData = p;
	}
}

EventData::~EventData() {
	if (m_OwnsEventInfo)
		::HeapFree(s_hHeap, 0, m_EventInfo);
	if (m_RecordData)
		::HeapFree(s_hHeap, 0, m_RecordData);
}

void* EventData::operator new(size_t size) {
	return ::HeapAlloc(s_hHeap, 0, size);
}

void EventData::operator delete(void* p) {
	::HeapFree(s_hHeap, 0, p);
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
	std::lock_guard locker(m_Lock);
	{
		if (m_PropertiesParsed || m_EventInfo == nullptr)
			return m_Properties;

		m_PropertiesParsed = true;
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
				auto p = m_Properties[len].GetData();
				auto plen = m_Properties[len].GetLength();
				assert(plen <= sizeof(ULONG) && plen > 0);
				if (plen > sizeof(ULONG))
					plen = sizeof(ULONG);
				len = 0;
				memcpy(&len, p, plen);
				if (len > userDataLength)
					len = userDataLength;
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
	if (!m_Strings._HasValue.load(std::memory_order_acquire)) {
		std::lock_guard locker(m_Lock);
		if (m_Strings._HasValue.load(std::memory_order_relaxed))
			return m_Strings;

		auto info = m_EventInfo;
		if (info) {
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
		m_Strings._HasValue.store(true, std::memory_order_release);
	}
	return m_Strings;
}

int EventData::GetCPU() const noexcept {
	return GetEventProcessorIndex(&m_Record);
}

std::vector<ULONG64> EventData::GetStackTrace() const {
	std::lock_guard locker(m_Lock);
	return m_Stack;
}

bool EventData::HasStackTrace() const noexcept {
	std::lock_guard locker(m_Lock);
	return !m_Stack.empty();
}

void EventData::SetStackTrace(std::vector<ULONG64> stack) {
	std::lock_guard locker(m_Lock);
	m_Stack = std::move(stack);
}

void EventData::AppendStackTrace(std::span<ULONG64 const> frames) {
	std::lock_guard locker(m_Lock);
	m_Stack.insert(m_Stack.end(), frames.begin(), frames.end());
}
