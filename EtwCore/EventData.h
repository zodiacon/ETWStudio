#pragma once

#include <tdh.h>
#include <evntcons.h>
#include <assert.h>

struct EventProperty {
	friend class EventData;

	EventProperty(EVENT_PROPERTY_INFO& info);

	std::wstring Name;
	EVENT_PROPERTY_INFO& Info;
	ULONG GetLength() const {
		return (ULONG)Data.size();
	}

	template<typename T>
	T GetValue() const {
		static_assert(std::is_trivially_copyable<T>() && !std::is_pointer<T>());
		assert(sizeof(T) == Data.size());
		return *(T*)Data.data();
	}

	BYTE* GetData() {
		return Data.data();
	}

	const BYTE* GetData() const {
		return Data.data();
	}

	PCWSTR GetUnicodeString() const;
	PCSTR GetAnsiString() const;

private:
	std::vector<BYTE> Data;
	void* Allocate(ULONG size);
};

class EventData {
	friend class TraceSession;
public:
	EventData(PEVENT_RECORD rec, std::wstring processName, const std::wstring& eventName, uint32_t index);

	void* operator new(size_t size);
	void operator delete(void* p);

	DWORD GetProcessId() const;
	DWORD GetThreadId() const;
	ULONGLONG GetTimeStamp() const;
	const GUID& GetProviderId() const;
	const EVENT_DESCRIPTOR& GetEventDescriptor() const;
	const std::wstring& GetProcessName() const;
	const std::wstring& GetEventName() const;
	uint32_t GetIndex() const;

	const std::vector<EventProperty>& GetProperties() const;
	const EventProperty* GetProperty(PCWSTR name) const;
	std::wstring FormatProperty(const EventProperty& prop) const;
	uint64_t GetEventKey() const;

protected:
	void SetProcessName(std::wstring name);

private:
	inline static HANDLE s_hHeap = nullptr;
	inline static CRITICAL_SECTION s_HeapLock = {0};
	inline static volatile uint32_t s_Count = 0;

	ULONG m_ThreadId, m_ProcessId;
	EVENT_DESCRIPTOR m_EventDescriptor;
	ULONGLONG _timeStamp;
	ULONG m_KernelTime, m_UserTime;
	GUID m_ProviderId;
	std::wstring m_ProcessName;
	USHORT m_HeaderFlags;
	const std::wstring& m_EventName;
	mutable std::unique_ptr<BYTE[]> m_Buffer;
	PEVENT_RECORD m_Record;
	mutable std::vector<EventProperty> m_Properties;
	uint32_t m_Index;
};

