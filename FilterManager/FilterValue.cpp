#include "pch.h"
#include "FilterValue.h"

FilterValue::FilterValue() = default;

FilterValue::FilterValue(int64_t value) : m_Value(value) {
}

FilterValue::FilterValue(std::string value) : m_Value(std::move(value)) {
}

FilterValue::FilterValue(std::wstring value) : m_Value(std::move(value)) {
}

FilterValue::FilterValue(NumericRange const& range) : m_Value(range) {
}

FilterValueType FilterValue::GetType() const {
    return FilterValueType(m_Value.index());
}

bool FilterValue::HasValue() const {
    return m_Value.index() != std::variant_npos;
}

std::wstring const& FilterValue::GetUnicodeString() const {
    return std::get<std::wstring>(m_Value);
}

std::string const& FilterValue::GetAnsiString() const {
    return std::get<std::string>(m_Value);
}

int64_t FilterValue::GetInteger() const {
    return std::get<int64_t>(m_Value);
}

