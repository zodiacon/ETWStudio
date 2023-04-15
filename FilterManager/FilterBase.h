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

class FilterBase abstract {
public:
	virtual FilterResult Eval(FilterValue const& value) = 0;
	virtual std::unique_ptr<FilterBase> Clone() const = 0;

	bool IsEnabled() const;
	void Enable(bool enable);
	
	CompareType GetCompareType() const;
	void SetCompareType(CompareType type);

	void SetResult(FilterResult result);
	FilterResult GetResult() const;

	FilterValue const& GetValue() const;
	void SetValue(FilterValue value);

protected:
	FilterBase();

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
