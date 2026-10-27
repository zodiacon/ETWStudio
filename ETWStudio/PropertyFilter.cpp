#include "pch.h"
#include "PropertyFilter.h"
#include <EventData.h>
#include "StringHelper.h"

namespace {
	const PropertyFilter::FieldInfo StandardFields[] = {
		{ L"Process ID", PropertyFilter::Field::ProcessId, true },
		{ L"Thread ID", PropertyFilter::Field::ThreadId, true },
		{ L"Process Name", PropertyFilter::Field::ProcessName, false },
		{ L"Index", PropertyFilter::Field::Index, true },
		{ L"Level", PropertyFilter::Field::Level, true },
		{ L"Event ID", PropertyFilter::Field::EventId, true },
		{ L"Event Name", PropertyFilter::Field::EventName, false },
		{ L"Task", PropertyFilter::Field::Task, false },
		{ L"Opcode", PropertyFilter::Field::Opcode, true },
		{ L"Keyword", PropertyFilter::Field::Keyword, false },
		{ L"Channel", PropertyFilter::Field::Channel, false },
		{ L"Provider", PropertyFilter::Field::Provider, false },
		{ L"Version", PropertyFilter::Field::Version, true },
		{ L"CPU", PropertyFilter::Field::CPU, true },
	};

	PropertyFilter::FieldInfo const* FindField(std::wstring const& name) {
		for (auto& f : StandardFields)
			if (_wcsicmp(f.Name, name.c_str()) == 0)
				return &f;
		return nullptr;
	}

	// decimal or hex (0x) integer; the entire text must parse
	std::optional<int64_t> ParseInteger(std::wstring const& text) {
		if (text.empty())
			return {};
		PWSTR end;
		errno = 0;
		auto value = ::wcstoll(text.c_str(), &end, 0);
		if (*end || errno == ERANGE) {
			// large unsigned hex values (e.g. keywords)
			errno = 0;
			auto uvalue = ::wcstoull(text.c_str(), &end, 0);
			if (*end || errno == ERANGE)
				return {};
			return (int64_t)uvalue;
		}
		return value;
	}

	std::optional<int64_t> ParseLevel(std::wstring const& text) {
		static const PCWSTR names[] = { L"Always", L"Critical", L"Error", L"Warning", L"Information", L"Verbose" };
		for (int i = 0; i < _countof(names); i++)
			if (_wcsicmp(names[i], text.c_str()) == 0)
				return i;
		return ParseInteger(text);
	}
}

using FieldInfo = PropertyFilter::FieldInfo;

std::span<FieldInfo const> PropertyFilter::GetStandardFields() {
	return StandardFields;
}

std::unique_ptr<PropertyFilter> PropertyFilter::Create(std::wstring name, CompareType compare, std::wstring const& text,
	FilterResult result, std::wstring& error) {
	if (name.empty()) {
		error = L"Missing property name";
		return nullptr;
	}
	auto filter = std::make_unique<PropertyFilter>(std::move(name));
	auto field = FindField(filter->m_Name);
	if (field && field->Numeric) {
		auto value = field->Type == Field::Level ? ParseLevel(text) : ParseInteger(text);
		if (!value) {
			error = std::format(L"'{}' requires a numeric value", field->Name);
			return nullptr;
		}
		filter->SetValue(*value);
	}
	else if (!field) {
		// event property: type unknown until evaluated; prefer a number if the text is one
		if (auto value = ParseInteger(text); value)
			filter->SetValue(*value);
		else
			filter->SetValue(text);
	}
	else {
		filter->SetValue(text);
	}
	filter->m_Text = text;
	filter->SetCompareType(compare);
	filter->SetResult(result);
	return filter;
}

PropertyFilter::PropertyFilter(std::wstring name) : m_Name(std::move(name)) {
	auto field = FindField(m_Name);
	m_Field = field ? field->Type : Field::Property;
	if (field)
		m_Name = field->Name;	// canonical casing
}

std::wstring const& PropertyFilter::GetName() const noexcept {
	return m_Name;
}

std::wstring const& PropertyFilter::GetText() const noexcept {
	return m_Text;
}

COLORREF PropertyFilter::GetColor() const noexcept {
	return m_Color;
}

void PropertyFilter::SetColor(COLORREF color) noexcept {
	m_Color = color;
}

std::optional<FilterValue> PropertyFilter::GetFieldValue(EventData const& evt) const {
	auto& desc = evt.GetEventDescriptor();
	switch (m_Field) {
		case Field::ProcessId: return (int64_t)evt.GetProcessId();
		case Field::ThreadId: return (int64_t)evt.GetThreadId();
		case Field::ProcessName: return evt.GetProcessName();
		case Field::Index: return (int64_t)evt.GetIndex();
		case Field::Level: return (int64_t)desc.Level;
		case Field::EventId: return (int64_t)desc.Id;
		case Field::EventName: return evt.GetEventStrings().Name;
		case Field::Task: return evt.GetEventStrings().Task;
		case Field::Opcode: return (int64_t)desc.Opcode;
		case Field::Keyword: return evt.GetEventStrings().Keyword;
		case Field::Channel: return evt.GetEventStrings().Channel;
		case Field::Provider: return StringHelper::ProviderGuidToName(evt.GetProviderId());
		case Field::Version: return (int64_t)desc.Version;
		case Field::CPU: return (int64_t)evt.GetCPU();
	}

	auto prop = evt.GetProperty(m_Name.c_str());
	if (!prop || prop->GetLength() == 0)
		return {};

	// arrays and structs: compare as displayed
	if (prop->IsArray || prop->IsStruct())
		return evt.FormatProperty(*prop);

	auto data = prop->GetData();
	auto len = prop->GetLength();
	switch (prop->Info.nonStructType.InType) {
		case TDH_INTYPE_INT8: return (int64_t)*(int8_t const*)data;
		case TDH_INTYPE_UINT8: return (int64_t)*(uint8_t const*)data;
		case TDH_INTYPE_INT16: if (len >= 2) return (int64_t)*(int16_t const*)data; break;
		case TDH_INTYPE_UINT16: if (len >= 2) return (int64_t)*(uint16_t const*)data; break;
		case TDH_INTYPE_INT32: if (len >= 4) return (int64_t)*(int32_t const*)data; break;
		case TDH_INTYPE_BOOLEAN:
		case TDH_INTYPE_UINT32:
		case TDH_INTYPE_HEXINT32: if (len >= 4) return (int64_t)*(uint32_t const*)data; break;
		case TDH_INTYPE_INT64:
		case TDH_INTYPE_UINT64:
		case TDH_INTYPE_HEXINT64:
		case TDH_INTYPE_FILETIME: if (len >= 8) return *(int64_t const*)data; break;
		case TDH_INTYPE_POINTER:
		case TDH_INTYPE_SIZET:
			if (len == 8) return *(int64_t const*)data;
			if (len == 4) return (int64_t)*(uint32_t const*)data;
			break;
		case TDH_INTYPE_UNICODESTRING:
			return std::wstring(prop->GetUnicodeString(), ::wcsnlen(prop->GetUnicodeString(), len / sizeof(WCHAR)));
		case TDH_INTYPE_ANSISTRING:
		{
			auto s = prop->GetAnsiString();
			auto n = ::strnlen(s, len);
			return std::wstring(s, s + n);
		}
	}
	// anything else (GUIDs, SIDs, addresses...): compare as displayed
	return evt.FormatProperty(*prop);
}

FilterResult PropertyFilter::Eval(FilterValue const& value) {
	if (value.GetType() != FilterValueType::Custom)
		return FilterResult::Passthrough;

	auto evt = value.GetAny<EventData*>();
	auto field = GetFieldValue(*evt);
	if (!field)
		return FilterResult::Passthrough;

	//
	// a number entered for a string property (or vice versa): compare as text
	//
	auto type = GetValue().GetType();
	if (field->GetType() == FilterValueType::UnicodeString && type == FilterValueType::Integer)
		return Compare(field->GetUnicodeString(), m_Text);
	if (field->GetType() == FilterValueType::Integer && type == FilterValueType::UnicodeString)
		return Compare(std::to_wstring(field->GetInteger()), m_Text);

	return StandardFilter::Eval(*field);
}

std::unique_ptr<FilterBase> PropertyFilter::Clone() const {
	auto filter = std::make_unique<PropertyFilter>(m_Name);
	CopyStateTo(*filter);
	filter->m_Text = m_Text;
	filter->m_Color = m_Color;
	return filter;
}
