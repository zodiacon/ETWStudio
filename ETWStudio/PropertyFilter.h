#pragma once

#include <StandardFilter.h>

class PropertyFilter : public StandardFilter {
public:
	explicit PropertyFilter(std::wstring propertyName);

	FilterResult Eval(FilterValue const& value) override;
	std::unique_ptr<FilterBase> Clone() const override;

private:
	std::wstring m_PropertyName;
};

