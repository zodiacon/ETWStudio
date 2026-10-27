#include "pch.h"
#include "FilterBase.h"

bool FilterBase::IsEnabled() const noexcept {
	return m_IsEnabled;
}

void FilterBase::Enable(bool enable) noexcept {
	m_IsEnabled = enable;
}

CompareType FilterBase::GetCompareType() const noexcept {
	return m_Compare;
}

void FilterBase::SetCompareType(CompareType type) noexcept {
	m_Compare = type;
}

void FilterBase::SetResult(FilterResult result) noexcept {
	m_Result = result;
}

FilterResult FilterBase::GetResult() const noexcept {
	return m_Result;
}

FilterValue const& FilterBase::GetValue() const noexcept {
	return m_Value;
}

void FilterBase::SetValue(FilterValue value) noexcept {
	m_Value = std::move(value);
}

void FilterBase::CopyStateTo(FilterBase& target) const {
	target.m_Name = m_Name;
	target.m_Result = m_Result;
	target.m_Value = m_Value;
	target.m_Compare = m_Compare;
	target.m_IsEnabled = m_IsEnabled;
}
