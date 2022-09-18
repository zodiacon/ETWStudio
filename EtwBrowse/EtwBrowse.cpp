// EtwBrowse.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <stdio.h>
#include <format>
#include "EtwProvider.h"

void DumpDescriptor(const EVENT_DESCRIPTOR& desc) {
	printf("Id: %u (0x%X) Version: %u Channel: %u Level: %u Opcode: %u Task: %u Keyword: 0x%llX\n",
		desc.Id, desc.Id, desc.Version, desc.Channel, desc.Level, desc.Opcode, desc.Task, desc.Keyword);
}

std::string InTypeToString(USHORT type) {
	switch(type) {
		case TDH_INTYPE_BOOLEAN: return "Boolean";
		case TDH_INTYPE_DOUBLE: return "Double";
		case TDH_INTYPE_ANSISTRING: return "Ansi String";
		case TDH_INTYPE_UNICODESTRING: return "Unicode String";
		case TDH_INTYPE_INT16: return "Int16";
		case TDH_INTYPE_INT32: return "Int32";
		case TDH_INTYPE_INT64: return "Int64";
		case TDH_INTYPE_INT8: return "Int8";
		case TDH_INTYPE_BINARY: return "Binary";
		case TDH_INTYPE_ANSICHAR: return "Ansi Char";
		case TDH_INTYPE_UNICODECHAR: return "Unicode Char";
		case TDH_INTYPE_FILETIME: return "FILETIME";
		case TDH_INTYPE_SYSTEMTIME: return "SYSTEMTIME";
		case TDH_INTYPE_COUNTEDSTRING: return "Counted String";
		case TDH_INTYPE_COUNTEDANSISTRING: return "Counted Ansi String";
		case TDH_INTYPE_FLOAT: return "Float";
		case TDH_INTYPE_GUID: return "GUID";
		case TDH_INTYPE_HEXINT32: return "Hex Int32";
		case TDH_INTYPE_HEXINT64: return "Hex Int64";
		case TDH_INTYPE_POINTER: return "Pointer";
		case TDH_INTYPE_SID: return "SID";
		case TDH_INTYPE_SIZET: return "SIZE_T";
		case TDH_INTYPE_UINT16: return "UInt16";
		case TDH_INTYPE_UINT32: return "UInt32";
		case TDH_INTYPE_UINT64: return "UInt64";
		case TDH_INTYPE_UINT8: return "UInt8";
	}

	return std::format("(%u)", type);
}

std::string OutTypeToString(USHORT type) {
	switch (type) {
		case TDH_OUTTYPE_BOOLEAN: return "Boolean";
		case TDH_OUTTYPE_DOUBLE: return "Double";
		case TDH_OUTTYPE_NULL: return "NULL";
		case TDH_OUTTYPE_INT: return "Int";
		case TDH_OUTTYPE_SHORT: return "Short";
		case TDH_OUTTYPE_BYTE: return "Byte";
		case TDH_OUTTYPE_UNSIGNEDBYTE: return "UByte";
		case TDH_OUTTYPE_UNSIGNEDINT: return "UInt";
		case TDH_OUTTYPE_UNSIGNEDLONG: return "ULong";
		case TDH_OUTTYPE_LONG: return "Long";
		case TDH_OUTTYPE_DATETIME: return "DateTime";
		case TDH_OUTTYPE_DATETIME_UTC: return "DateTime UTC";
		case TDH_OUTTYPE_CODE_POINTER: return "Code Pointer";
		case TDH_OUTTYPE_ERRORCODE: return "Error Code";
		case TDH_OUTTYPE_ETWTIME: return "ETW Time";
		case TDH_OUTTYPE_FLOAT: return "Float";
		case TDH_OUTTYPE_GUID: return "GUID";
		case TDH_OUTTYPE_HEXINT32: return "Hex Int32";
		case TDH_OUTTYPE_HEXINT64: return "Hex Int64";
		case TDH_OUTTYPE_HEXBINARY: return "Hex Binary";
		case TDH_OUTTYPE_CULTURE_INSENSITIVE_DATETIME: return "CI DateTime";
		case TDH_OUTTYPE_HEXINT16: return "Hex Int16";
		case TDH_OUTTYPE_HRESULT: return "HRESULT";
		case TDH_OUTTYPE_IPV4: return "IPv4";
		case TDH_OUTTYPE_IPV6: return "IPv6";
		case TDH_OUTTYPE_JSON: return "JSON";
		case TDH_OUTTYPE_NTSTATUS: return "NTSTATUS";
		case TDH_OUTTYPE_NOPRINT: return "No Print";
		case TDH_OUTTYPE_PID: return "PID";
		case TDH_OUTTYPE_STRING: return "String";
		case TDH_OUTTYPE_SOCKETADDRESS: return "Socket Address";
		case TDH_OUTTYPE_PORT: return "Port";
		case TDH_OUTTYPE_TID: return "TID";
		case TDH_OUTTYPE_UNSIGNEDSHORT: return "UShort";
	}
	return std::format("(%u)", type);
}

std::wstring PropertyFlagsToString(EtwPropertyFlags flags) {
	static const struct {
		PCWSTR text;
		EtwPropertyFlags flag;
	} data[] = {
		{ L"Struct", EtwPropertyFlags::Struct },
		{ L"Param Length", EtwPropertyFlags::ParamLength },
		{ L"Param Count", EtwPropertyFlags::ParamCount },
		{ L"XML Fragment", EtwPropertyFlags::WBEMXmlFragment } ,
		{ L"Param Fixed Length", EtwPropertyFlags::ParamFixedLength },
		{ L"Param Fixed Count", EtwPropertyFlags::ParamFixedCount },
		{ L"Has Tags", EtwPropertyFlags::HasTags },
		{ L"Has Custom Schema", EtwPropertyFlags::HasCustomSchema },
	};

	std::wstring result;
	for (auto& d : data) {
		if ((d.flag & flags) == d.flag)
			(result += d.text) += L", ";
	}
	if (result.empty())
		return L"None";
	return result.substr(0, result.length() - 2);
}

void DumpProperty(const EtwEventProperty& prop) {
	printf("\tProperty: %ws Flags: %ws InType: %s OutType: %s\n", 
		prop.Name.c_str(), PropertyFlagsToString(prop.Flags).c_str(), 
		InTypeToString(prop.InType).c_str(), OutTypeToString(prop.OutType).c_str());
}

void DumpProvider(const EtwProvider& provider, bool includeEvents, bool includeProperties) {
	printf("Provider Name: %ws\n", provider.Name().c_str());
	printf("Provider GUID: %ws\n", provider.GuidAsString().c_str());
	printf("Manifest Source: %s\n", provider.SchemaSource() == EtwSchemaSource::Mof ? "MOF" : "XML");

	if (includeEvents) {
		auto events = provider.GetProviderEvents();
		printf("Events: %u\n", (unsigned)events.size());
		for (auto& evt : events) {
			auto info = provider.EventInfo(evt);
			DumpDescriptor(evt);
			if (!info.EventName.empty())
				printf("Event Name: %ws\n", info.EventName.c_str());
			if (!info.KeywordName.empty())
				printf("Keyword: %ws\n", info.KeywordName.c_str());
			if (!info.TaskName.empty())
				printf("Task: %ws\n", info.TaskName.c_str());
			if (!info.ChannelName.empty())
				printf("Channel: %ws\n", info.ChannelName.c_str());
			if (!info.LevelName.empty())
				printf("Level: %ws\n", info.LevelName.c_str());
			if (!info.OpCodeName.empty())
				printf("Opcode: %ws\n", info.OpCodeName.c_str());
			if (!info.ProviderMessage.empty())
				printf("Provider Message: %ws\n", info.ProviderMessage.c_str());
			if (!info.EventMessage.empty())
				printf("Event Message:\n%ws\n", info.EventMessage.c_str());

			printf("Property Count: %u\n", (unsigned)info.Properties.size());
			if (includeProperties) {
				for (auto& prop : info.Properties) {
					DumpProperty(prop);
				}
			}
			printf("\n");
		}
	}
}

void DumpAllProviders() {
	int count = printf("%-60s %-38s %s\n", "Provider Name", "Provider GUID", "Schema Source");
	printf("%s\n", std::string('-', count).c_str());

	for (auto& provider : EtwProvider::EnumProviders(true)) {
		printf("%-60ws %-38ws %s\n", provider.Name().c_str(), 
			provider.GuidAsString().c_str(), provider.SchemaSource() == EtwSchemaSource::Mof ? "MOF" : "XML");
	}
}

void DumpProviders(const std::wstring& filter, bool includeProperties) {
	std::wstring search(filter);
	_wcslwr_s(search.data(), search.length() + 1);
	for (auto& provider : EtwProvider::EnumProviders(true)) {
		auto name(provider.Name());
		_wcslwr_s(name.data(), name.length() + 1);
		if (name.find(search) != name.npos) {
			DumpProvider(provider, true, includeProperties);
			printf("\n");
		}
	}
}

int wmain(int argc, const wchar_t* argv[]) {
	if (argc == 1) {
		DumpAllProviders();
		return 0;
	}

	DumpProviders(argv[1], true);

	return 0;
}

