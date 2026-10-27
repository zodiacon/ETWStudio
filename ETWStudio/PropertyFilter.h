#pragma once

#include <StandardFilter.h>
#include <optional>
#include <span>

class EventData;

//
// filters events (FilterValue holding an EventData*) by a standard event field
// (e.g. "Process ID") or, if the name is not a standard field, by a named event property
//
class PropertyFilter : public StandardFilter {
public:
	enum class Field {
		Property,		// event property by name
		ProcessId, ThreadId, ProcessName, Index, Level, EventId, EventName,
		Task, Opcode, Keyword, Channel, Provider, Version, CPU,
	};

	struct FieldInfo {
		PCWSTR Name;
		Field Type;
		bool Numeric;
	};
	static std::span<FieldInfo const> GetStandardFields();

	// parses text into the filter value; returns nullptr and sets error if the text is invalid for the field
	static std::unique_ptr<PropertyFilter> Create(std::wstring name, CompareType compare, std::wstring const& text,
		FilterResult result, std::wstring& error);

	explicit PropertyFilter(std::wstring name);

	FilterResult Eval(FilterValue const& value) override;
	std::unique_ptr<FilterBase> Clone() const override;

	std::wstring const& GetName() const noexcept;
	std::wstring const& GetText() const noexcept;

	// highlight rules: the row background color
	COLORREF GetColor() const noexcept;
	void SetColor(COLORREF color) noexcept;

private:
	std::optional<FilterValue> GetFieldValue(EventData const& evt) const;

	std::wstring m_Name;
	std::wstring m_Text;	// the value as entered
	Field m_Field;
	COLORREF m_Color{ CLR_INVALID };
};
