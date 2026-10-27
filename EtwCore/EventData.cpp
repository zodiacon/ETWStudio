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
			m_StackMatchId = stack->MatchId;
		}
		else if (item.ExtType == EVENT_HEADER_EXT_TYPE_STACK_TRACE32) {
			auto count = (item.DataSize - sizeof(ULONG64)) / sizeof(ULONG);
			auto stack = reinterpret_cast<PEVENT_EXTENDED_ITEM_STACK_TRACE32>(item.DataPtr);
			m_Stack.assign(stack->Address, stack->Address + count);
			m_StackMatchId = stack->MatchId;
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

namespace {
	// in-types whose element size is EVENT_PROPERTY_INFO::length bytes
	bool IsFixedSize(USHORT inType) {
		switch (inType) {
			case TDH_INTYPE_INT8: case TDH_INTYPE_UINT8:
			case TDH_INTYPE_INT16: case TDH_INTYPE_UINT16:
			case TDH_INTYPE_INT32: case TDH_INTYPE_UINT32:
			case TDH_INTYPE_INT64: case TDH_INTYPE_UINT64:
			case TDH_INTYPE_FLOAT: case TDH_INTYPE_DOUBLE:
			case TDH_INTYPE_BOOLEAN: case TDH_INTYPE_BINARY:
			case TDH_INTYPE_HEXINT32: case TDH_INTYPE_HEXINT64:
			case TDH_INTYPE_FILETIME: case TDH_INTYPE_SYSTEMTIME:
			case TDH_INTYPE_GUID:
			case TDH_INTYPE_POINTER: case TDH_INTYPE_SIZET:
				return true;
		}
		return false;
	}

	// longest array shown by FormatProperty
	const ULONG MaxFormattedElements = 64;
}

ULONG EventData::GetPropertyValueAsCount(ULONG index) const noexcept {
	if (index >= m_Properties.size())
		return 0;
	auto& prop = m_Properties[index];
	ULONG value = 0;
	memcpy(&value, prop.GetData(), (std::min)(prop.GetLength(), (ULONG)sizeof(value)));
	return value;
}

const std::vector<EventProperty>& EventData::GetProperties() const {
	std::lock_guard locker(m_Lock);
	if (m_PropertiesParsed || m_EventInfo == nullptr)
		return m_Properties;

	m_PropertiesParsed = true;
	auto info = m_EventInfo;
	m_Properties.reserve(info->TopLevelPropertyCount);
	ULONG remaining = m_Record.UserDataLength;
	auto data = (BYTE const*)m_Record.UserData;
	ULONG pointerSize = (m_Header.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? 4 : 8;

	for (ULONG i = 0; i < info->TopLevelPropertyCount; i++) {
		auto& prop = info->EventPropertyInfoArray[i];
		EventProperty property(prop);
		property.Name.assign((WCHAR*)((BYTE*)info + prop.NameOffset));

		// count and length may be the values of earlier properties
		property.Count = (prop.Flags & PropertyParamCount) ? GetPropertyValueAsCount(prop.countPropertyIndex) : prop.count;
		property.IsArray = (prop.Flags & (PropertyParamCount | PropertyParamFixedCount)) || property.Count > 1;
		bool isStruct = property.IsStruct();
		if (!isStruct) {
			property.ElementLength = (prop.Flags & PropertyParamLength) ? GetPropertyValueAsCount(prop.lengthPropertyIndex) : prop.length;
			if (prop.nonStructType.InType == TDH_INTYPE_POINTER || prop.nonStructType.InType == TDH_INTYPE_SIZET)
				property.ElementLength = pointerSize;
		}

		ULONG size = 0;
		bool known = false;
		if (!isStruct && property.ElementLength > 0 && IsFixedSize(prop.nonStructType.InType)) {
			size = property.ElementLength * property.Count;
			known = true;
		}
		else {
			// strings, SIDs, structs...: let TDH compute the size (of the whole array)
			PROPERTY_DATA_DESCRIPTOR desc{};
			desc.PropertyName = (ULONGLONG)property.Name.c_str();
			desc.ArrayIndex = ULONG_MAX;
			known = ::TdhGetPropertySize((PEVENT_RECORD)&m_Record, 0, nullptr, 1, &desc, &size) == ERROR_SUCCESS;
		}
		if (!known) {
			// fall back to walking the elements by formatting them
			auto p = data;
			auto left = remaining;
			auto map = GetMapInfo(prop);
			std::wstring text;
			known = true;
			for (ULONG e = 0; e < property.Count && known; e++) {
				if (isStruct)
					known = FormatStruct(prop, p, left, text);
				else {
					USHORT consumed = 0;
					known = FormatValue(prop, (PEVENT_MAP_INFO)map.get(), property.ElementLength, p, left, text, consumed);
					p += consumed;
					left -= consumed;
				}
			}
			size = remaining - left;
		}
		if (!known || (size > 0 && remaining == 0))
			break;		// the rest of the data can't be located reliably, or there is none (older event version)

		size = (std::min)(size, remaining);		// truncated event
		if (size) {
			::memcpy(property.Allocate(size), data, size);
			data += size;
			remaining -= size;
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

std::unique_ptr<BYTE[]> EventData::GetMapInfo(EVENT_PROPERTY_INFO const& prop) const {
	if ((prop.Flags & PropertyStruct) || prop.nonStructType.MapNameOffset == 0)
		return nullptr;

	auto mapName = (PWSTR)((PBYTE)m_EventInfo + prop.nonStructType.MapNameOffset);
	ULONG size = 0;
	if (ERROR_INSUFFICIENT_BUFFER != ::TdhGetEventMapInformation((PEVENT_RECORD)&m_Record, mapName, nullptr, &size))
		return nullptr;
	auto buffer = std::make_unique<BYTE[]>(size);
	if (ERROR_SUCCESS != ::TdhGetEventMapInformation((PEVENT_RECORD)&m_Record, mapName, (PEVENT_MAP_INFO)buffer.get(), &size))
		return nullptr;
	return buffer;
}

bool EventData::FormatValue(EVENT_PROPERTY_INFO const& prop, PEVENT_MAP_INFO map, ULONG length, BYTE const* data, ULONG size, std::wstring& text, USHORT& consumed) const {
	consumed = 0;
	text.clear();
	if (size == 0)
		return true;

	auto inType = prop.nonStructType.InType;
	auto outType = prop.nonStructType.OutType;
	// length special case for IPV6
	if (inType == TDH_INTYPE_BINARY && outType == TDH_OUTTYPE_IPV6)
		length = sizeof(IN6_ADDR);

	ULONG bufferSize = 128 * sizeof(WCHAR);
	for (int i = 0; i < 4; i++) {
		text.resize(bufferSize / sizeof(WCHAR));
		auto status = ::TdhFormatProperty(m_EventInfo, map, (m_Header.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER) ? 4 : 8,
			inType, outType, (USHORT)length, (USHORT)(std::min)(size, (ULONG)USHRT_MAX), (PBYTE)data, &bufferSize, text.data(), &consumed);
		if (status == ERROR_INSUFFICIENT_BUFFER)
			continue;		// bufferSize now has the required size
		if (status != ERROR_SUCCESS)
			break;
		text.resize(::wcsnlen(text.c_str(), text.size()));
		return consumed <= size;
	}
	text.clear();
	consumed = 0;
	return false;
}

bool EventData::FormatStruct(EVENT_PROPERTY_INFO const& prop, BYTE const*& data, ULONG& size, std::wstring& text) const {
	auto info = m_EventInfo;
	auto first = prop.structType.StructStartIndex;
	auto members = prop.structType.NumOfStructMembers;
	if ((ULONG)first + members > info->PropertyCount)
		return false;

	// numeric member values, for members whose count/length refers to a sibling
	std::vector<ULONG> values(members);
	auto valueOf = [&](USHORT index) {
		return index >= first && index < first + members ? values[index - first] : GetPropertyValueAsCount(index);
	};

	std::wstring value;
	text += L"{";
	for (USHORT m = 0; m < members; m++) {
		auto& member = info->EventPropertyInfoArray[first + m];
		if (member.Flags & PropertyStruct)
			return false;
		if (m)
			text += L", ";
		text += (PCWSTR)((BYTE*)info + member.NameOffset);
		text += L": ";

		ULONG count = (member.Flags & PropertyParamCount) ? valueOf(member.countPropertyIndex) : member.count;
		bool isArray = (member.Flags & (PropertyParamCount | PropertyParamFixedCount)) || count > 1;
		ULONG length = (member.Flags & PropertyParamLength) ? valueOf(member.lengthPropertyIndex) : member.length;
		auto map = GetMapInfo(member);
		if (isArray)
			text += L"[";
		for (ULONG e = 0; e < count; e++) {
			USHORT consumed;
			if (!FormatValue(member, (PEVENT_MAP_INFO)map.get(), length, data, size, value, consumed))
				return false;
			if (e < MaxFormattedElements)
				text += (e ? L", " : L"") + value;
			else if (e == MaxFormattedElements)
				text += L", ...";
			if (e == 0 && consumed <= sizeof(ULONG))
				memcpy(&values[m], data, consumed);
			data += consumed;
			size -= consumed;
		}
		if (isArray)
			text += L"]";
	}
	text += L"}";
	return true;
}

std::wstring EventData::FormatProperty(const EventProperty& property) const {
	if (m_EventInfo == nullptr)
		return L"";

	auto& prop = property.Info;
	BYTE const* data = property.GetData();
	auto size = property.GetLength();
	auto map = GetMapInfo(prop);
	std::wstring text, value;
	if (property.IsArray)
		text = L"[";
	for (ULONG i = 0; i < property.Count && size > 0; i++) {
		if (i == MaxFormattedElements) {
			text += L", ...";
			break;
		}
		if (i)
			text += L", ";
		if (property.IsStruct()) {
			if (!FormatStruct(prop, data, size, text))
				break;
		}
		else {
			USHORT consumed;
			if (!FormatValue(prop, (PEVENT_MAP_INFO)map.get(), property.ElementLength, data, size, value, consumed))
				break;
			text += value;
			if (consumed == 0)
				break;
			data += consumed;
			size -= consumed;
		}
	}
	if (property.IsArray)
		text += L"]";
	return text;
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

ULONG64 EventData::GetStackMatchId() const noexcept {
	return m_StackMatchId;
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
