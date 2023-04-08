#pragma once

enum class EtwDecodingSource;

struct StringHelper abstract final {
	static std::wstring GuidToString(GUID const& guid);
	static PCWSTR DecodingSourceToString(EtwDecodingSource source);
	static std::wstring OutTypeToString(USHORT type);
	static std::wstring InTypeToString(USHORT type);
	static PCWSTR LevelToString(UCHAR level);
	static std::wstring TimeStampToString(ULONGLONG ts);
};

