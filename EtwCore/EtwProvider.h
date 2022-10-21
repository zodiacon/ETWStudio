#pragma once

#include <tdh.h>
#include <vector>
#include <string>
#include <memory>

enum class EtwSchemaSource {
	Xml = 0,
	Mof = 1,
};

enum class EtwPropertyFlags {
	None = 0,
	Struct = 0x1,				// Type is struct.
	ParamLength = 0x2,			// Length field is index of param with length.
	ParamCount = 0x4,			// Count field is index of param with count.
	WBEMXmlFragment = 0x8,		// WBEM extension flag for property.
	ParamFixedLength = 0x10,	// Length of the parameter is fixed.
	ParamFixedCount = 0x20,		// Count of the parameter is fixed.
	HasTags = 0x40,				// The Tags field has been initialized.
	HasCustomSchema = 0x80,		// Type is described with a custom schema.
};
DEFINE_ENUM_FLAG_OPERATORS(EtwPropertyFlags);

struct EtwEventProperty {
	EtwPropertyFlags Flags;
	std::wstring Name;
	USHORT InType;
	USHORT OutType;
	std::wstring MapName;
};

enum class EtwDecodingSource {
	XMLFile,
	Wbem,
	WPP,
	Tlg,
	Max
};

struct EtwEventInfo {
	GUID ProviderGuid;
	GUID EventGuid;
	EVENT_DESCRIPTOR Descriptor;
	EtwDecodingSource DescodingSource;
	std::wstring ProviderName;
	std::wstring LevelName;
	std::wstring ChannelName;
	std::wstring KeywordName;
	std::wstring TaskName;
	std::wstring OpCodeName;
	std::wstring ProviderMessage;
	std::wstring EventMessage;
	std::wstring EventName;
	std::wstring EventAttributes;
	std::vector<BYTE> BinaryXML;
	ULONG Tags;
	std::vector<EtwEventProperty> Properties;
};

enum class EtwFieldType {
	KeywordInformation,
	LevelInformation,
	ChannelInformation,
	TaskInformation,
	OpcodeInformation,
};

struct EtwFieldInfo {
	EtwFieldType Type;
	std::wstring Name;
	std::wstring Desc;
	ULONGLONG Value;
};

class EtwProvider final {
public:
	EtwProvider(GUID const& guid, PCWSTR name, EtwSchemaSource source);

	static std::vector<EtwProvider> EnumProviders(bool sort = false);
	static std::vector<std::unique_ptr<EtwProvider>> EnumProviders2(bool sort = false);

	const std::wstring& Name() const;
	const std::wstring& GuidAsString() const;
	const GUID& Guid() const;
	EtwSchemaSource SchemaSource() const;
	std::vector<EVENT_DESCRIPTOR> const& GetProviderEvents() const;
	EtwEventInfo EventInfo(const EVENT_DESCRIPTOR& desc) const;
	int32_t EventCount() const;

	std::vector<EtwFieldInfo> FieldInfo(EtwFieldType type) const;

private:
	GUID m_guid;
	std::wstring m_name, m_guidString;
	EtwSchemaSource m_source;
	mutable std::vector<EVENT_DESCRIPTOR> m_events;
	mutable int32_t m_eventCount{ -1 };
};

