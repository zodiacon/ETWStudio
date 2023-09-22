#pragma once

#include "StandardFilter.h"
#include <functional>

class ProviderFilter : public StandardFilter {
public:
	explicit ProviderFilter(std::function<FilterValue()> f);
	FilterResult Eval(FilterValue const& value) override;
	std::unique_ptr<FilterBase> Clone() const override;

private:
	std::is_function<FilterValue()> m_Func;
};

