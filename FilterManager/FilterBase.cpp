#include "pch.h"
#include "FilterBase.h"

FilterBase::FilterBase() = default;

bool FilterBase::IsEnabled() const {
	return m_IsEnabled;
}

void FilterBase::Enable(bool enable) {
	m_IsEnabled = enable;
}

CompareType FilterBase::GetCompareType() const {
	return m_Compare;
}

void FilterBase::SetCompareType(CompareType type) {
	m_Compare = type;
}

void FilterBase::SetResult(FilterResult result) {
	m_Result = result;
}

FilterResult FilterBase::GetResult() const {
	return m_Result;
}

FilterValue const& FilterBase::GetValue() const {
	return m_Value;
}

void FilterBase::SetValue(FilterValue value) {
	m_Value = std::move(value);
}
