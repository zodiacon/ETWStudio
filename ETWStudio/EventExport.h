#pragma once

class EventData;

// buffered UTF-8 file output
class Utf8Writer {
public:
	explicit Utf8Writer(PCWSTR path) : m_File(::CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr)) {
	}
	~Utf8Writer() {
		Flush();
	}
	bool IsOpen() const {
		return m_File.is_valid();
	}
	bool Ok() const {
		return m_Ok;
	}
	Utf8Writer& operator<<(std::wstring_view text) {
		m_Buffer += text;
		if (m_Buffer.size() > (1 << 20))
			Flush();
		return *this;
	}
	bool Flush() {
		if (m_Buffer.empty() || !m_Ok)
			return m_Ok;
		auto size = ::WideCharToMultiByte(CP_UTF8, 0, m_Buffer.data(), (int)m_Buffer.size(), nullptr, 0, nullptr, nullptr);
		std::string utf8(size, 0);
		::WideCharToMultiByte(CP_UTF8, 0, m_Buffer.data(), (int)m_Buffer.size(), utf8.data(), size, nullptr, nullptr);
		DWORD written;
		m_Ok = ::WriteFile(m_File.get(), utf8.data(), (DWORD)utf8.size(), &written, nullptr) && written == utf8.size();
		m_Buffer.clear();
		return m_Ok;
	}

private:
	wil::unique_hfile m_File;
	std::wstring m_Buffer;
	bool m_Ok{ true };
};

namespace EventExport {
	// quotes a CSV field if needed
	std::wstring CsvField(std::wstring_view text);
	// an event's stack frames' symbols, in order
	using StackSymbols = std::function<std::vector<std::wstring>(EventData const&)>;
	// all event data (header fields, properties, stack) as a JSON array.
	// progress (optional) gets the number of events written so far; returning false stops (returns false).
	// symbols (optional) adds "StackSymbols" next to "Stack"
	bool WriteJson(Utf8Writer& writer, std::span<EventData* const> events, std::function<bool(size_t)> const& progress = nullptr,
		StackSymbols const& symbols = nullptr);
}
