#include "pch.h"
#include "StringHelpers.h"
#include <EtwProvider.h>

std::wstring StringHelpers::GuidToString(GUID const& guid) {
    WCHAR sguid[64];
    return ::StringFromGUID2(guid, sguid, _countof(sguid)) ? sguid : L"";
}

PCWSTR StringHelpers::DecodingSourceToString(EtwDecodingSource source) {
	switch (source) {
		case EtwDecodingSource::XMLFile: return L"XML";
		case EtwDecodingSource::Wbem: return L"WBEM";
		case EtwDecodingSource::WPP: return L"WPP";
		case EtwDecodingSource::Tlg: return L"TLG";
	}
	return L"";
}

std::wstring StringHelpers::OutTypeToString(USHORT type) {
	switch (type) {
		case TDH_OUTTYPE_BOOLEAN: return L"Boolean";
		case TDH_OUTTYPE_DOUBLE: return L"Double";
		case TDH_OUTTYPE_NULL: return L"NULL";
		case TDH_OUTTYPE_INT: return L"Int";
		case TDH_OUTTYPE_SHORT: return L"Short";
		case TDH_OUTTYPE_BYTE: return L"Byte";
		case TDH_OUTTYPE_UNSIGNEDBYTE: return L"UByte";
		case TDH_OUTTYPE_UNSIGNEDINT: return L"UInt";
		case TDH_OUTTYPE_UNSIGNEDLONG: return L"ULong";
		case TDH_OUTTYPE_LONG: return L"Long";
		case TDH_OUTTYPE_DATETIME: return L"DateTime";
		case TDH_OUTTYPE_DATETIME_UTC: return L"DateTime UTC";
		case TDH_OUTTYPE_CODE_POINTER: return L"Code Pointer";
		case TDH_OUTTYPE_ERRORCODE: return L"Error Code";
		case TDH_OUTTYPE_ETWTIME: return L"ETW Time";
		case TDH_OUTTYPE_FLOAT: return L"Float";
		case TDH_OUTTYPE_GUID: return L"GUID";
		case TDH_OUTTYPE_HEXINT32: return L"Hex Int32";
		case TDH_OUTTYPE_HEXINT64: return L"Hex Int64";
		case TDH_OUTTYPE_HEXBINARY: return L"Hex Binary";
		case TDH_OUTTYPE_CULTURE_INSENSITIVE_DATETIME: return L"CI DateTime";
		case TDH_OUTTYPE_HEXINT16: return L"Hex Int16";
		case TDH_OUTTYPE_HRESULT: return L"HRESULT";
		case TDH_OUTTYPE_IPV4: return L"IPv4";
		case TDH_OUTTYPE_IPV6: return L"IPv6";
		case TDH_OUTTYPE_JSON: return L"JSON";
		case TDH_OUTTYPE_NTSTATUS: return L"NTSTATUS";
		case TDH_OUTTYPE_NOPRINT: return L"No Print";
		case TDH_OUTTYPE_PID: return L"PID";
		case TDH_OUTTYPE_STRING: return L"String";
		case TDH_OUTTYPE_SOCKETADDRESS: return L"Socket Address";
		case TDH_OUTTYPE_PORT: return L"Port";
		case TDH_OUTTYPE_TID: return L"TID";
		case TDH_OUTTYPE_UNSIGNEDSHORT: return L"UShort";
	}
	return std::format(L"(%u)", type);
}

std::wstring StringHelpers::InTypeToString(USHORT type) {
	switch (type) {
		case TDH_INTYPE_BOOLEAN: return L"Boolean";
		case TDH_INTYPE_DOUBLE: return L"Double";
		case TDH_INTYPE_ANSISTRING: return L"Ansi String";
		case TDH_INTYPE_UNICODESTRING: return L"Unicode String";
		case TDH_INTYPE_INT16: return L"Int16";
		case TDH_INTYPE_INT32: return L"Int32";
		case TDH_INTYPE_INT64: return L"Int64";
		case TDH_INTYPE_INT8: return L"Int8";
		case TDH_INTYPE_BINARY: return L"Binary";
		case TDH_INTYPE_ANSICHAR: return L"Ansi Char";
		case TDH_INTYPE_UNICODECHAR: return L"Unicode Char";
		case TDH_INTYPE_FILETIME: return L"FILETIME";
		case TDH_INTYPE_SYSTEMTIME: return L"SYSTEMTIME";
		case TDH_INTYPE_COUNTEDSTRING: return L"Counted String";
		case TDH_INTYPE_COUNTEDANSISTRING: return L"Counted Ansi String";
		case TDH_INTYPE_FLOAT: return L"Float";
		case TDH_INTYPE_GUID: return L"GUID";
		case TDH_INTYPE_HEXINT32: return L"Hex Int32";
		case TDH_INTYPE_HEXINT64: return L"Hex Int64";
		case TDH_INTYPE_POINTER: return L"Pointer";
		case TDH_INTYPE_SID: return L"SID";
		case TDH_INTYPE_SIZET: return L"SIZE_T";
		case TDH_INTYPE_UINT16: return L"UInt16";
		case TDH_INTYPE_UINT32: return L"UInt32";
		case TDH_INTYPE_UINT64: return L"UInt64";
		case TDH_INTYPE_UINT8: return L"UInt8";
	}

	return std::format(L"(%u)", type);
}

PCWSTR StringHelpers::LevelToString(UCHAR level) {
	switch (level) {
		case 0: return L"Always";
		case 1: return L"Critical";
		case 2: return L"Error";
		case 3: return L"Warning";
		case 4: return L"Information";
		case 5: return L"Verbose";
	}
	ATLASSERT(false);
	return L"(Unknown)";
}
