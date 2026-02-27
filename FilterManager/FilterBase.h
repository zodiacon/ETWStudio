#pragma once

#include "FilterValue.h"

enum class FilterResult {
	Passthrough,
	Include,
	Exclude,
};

enum class CompareType {
	Equal,
	NotEqual,
	LessThan,
	GreaterThan,
	LessThanOrEqual,
	GreaterThanOrEqual,
	Contains,
	NotContains,
	StartsWith,
	EndsWith,
	InRange,
	NotInRange,
};

class FilterBase {
public:
	virtual FilterResult Eval(FilterValue const& value) = 0;
	virtual std::unique_ptr<FilterBase> Clone() const = 0;

	bool IsEnabled() const noexcept;
	void Enable(bool enable) noexcept;
	
	CompareType GetCompareType() const noexcept;
	void SetCompareType(CompareType type) noexcept;

	void SetResult(FilterResult result) noexcept;
	FilterResult GetResult() const noexcept;

	FilterValue const& GetValue() const noexcept;
	void SetValue(FilterValue value) noexcept;

protected:
	FilterBase() = default;

private:
	std::wstring m_Name;
	FilterResult m_Result{ FilterResult::Passthrough };
	FilterValue m_Value;
	CompareType m_Compare;
	bool m_IsEnabled{ true };
};

enum class FilterCombineType {
	And,
	Or,
	Xor,
};

class CombinedFilter final : public FilterBase {
public:
	CombinedFilter(FilterBase* left, FilterBase* right, FilterCombineType type);

private:
	FilterCombineType m_Type;
};
