#pragma once

#include "FilterBase.h"

class StandardFilter : public FilterBase {
public:
	FilterResult Eval(FilterValue const& value) override;
	std::unique_ptr<FilterBase> Clone() const override;

protected:
	FilterResult Compare(int64_t left, int64_t right) const;
	FilterResult Compare(std::wstring const& left, std::wstring const& right) const;
	FilterResult Compare(std::string const& left, std::string const& right) const;

};

