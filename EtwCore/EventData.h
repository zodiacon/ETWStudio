#pragma once

#include <tdh.h>
#include <evntcons.h>
#include <assert.h>

struct EventStrings {
	std::wstring Name;
	std::wstring Keyword;
	std::wstring Channel;
	std::wstring Message;
	std::wstring Task;
	std::wstring Opcode;
	std::wstring Level;
	std::wstring ProviderMessage;
	std::wstring EventAttributes;
	std::wstring RelatedActivity;
	std::atomic<bool> _HasValue{ false };
};

struct EventProperty {
	friend class EventData;

	explicit EventProperty(EVENT_PROPERTY_INFO& info);

	std::wstring Name;
	EVENT_PROPERTY_INFO const& Info;

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

class EventData final {
	friend class TraceSession;
public:
	EventData(PEVENT_RECORD rec, std::wstring processName, uint32_t index);
	EventData(EventData const&) = delete;
	EventData& operator=(EventData const&) = delete;

	~EventData();

	void* operator new(size_t size);
	void operator delete(void* p);

	DWORD GetProcessId() const;
	DWORD GetThreadId() const;
	EVENT_HEADER const& GetEventHeader() const;
	ULONGLONG GetTimeStamp() const noexcept;
	const GUID& GetProviderId() const noexcept;
	const EVENT_DESCRIPTOR& GetEventDescriptor() const;
	const std::wstring& GetProcessName() const;
	uint32_t GetIndex() const noexcept;
	EventStrings const& GetEventStrings() const;
	int GetCPU() const noexcept;
	const std::vector<EventProperty>& GetProperties() const;
	const EventProperty* GetProperty(PCWSTR name) const noexcept;
	std::wstring FormatProperty(const EventProperty& prop) const;

protected:
	void SetProcessName(std::wstring name);

private:
	inline static HANDLE s_hHeap = ::HeapCreate(0, 1 << 24, 0);

	EVENT_HEADER m_Header;
	std::wstring m_ProcessName;
	DWORD m_ThreadId{ 0 }, m_ProcessId{ 0 };
	mutable std::vector<EventProperty> m_Properties;
	uint32_t m_Index;
	mutable EventStrings m_Strings;
	PTRACE_EVENT_INFO m_EventInfo{};
	EVENT_RECORD m_Record;
	mutable std::mutex m_Lock;
};

