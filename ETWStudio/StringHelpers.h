#pragma once

enum class EtwDecodingSource;

struct StringHelpers abstract final {
	static std::wstring GuidToString(GUID const& guid);
	static PCWSTR DecodingSourceToString(EtwDecodingSource source);
	static std::wstring OutTypeToString(USHORT type);
	static std::wstring InTypeToString(USHORT type);
};

