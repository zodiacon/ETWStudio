#pragma once

#include "FilterBase.h"
#include <span>
#include <ranges>

class FilterManager final {
public:
	FilterManager() = default;
	FilterManager(FilterManager const&) = delete;
	FilterManager& operator=(FilterManager const&) = delete;
	FilterManager(FilterManager&&) = default;
	FilterManager& operator=(FilterManager&&) = default;

	FilterManager Clone() const;

	FilterResult Eval(FilterValue const& value) noexcept;

	void AddFilter(std::unique_ptr<FilterBase> filter);
	void SetDefaultResult(FilterResult result);
	FilterResult GetDefaultReesult() const;

	void Clear();
	bool SwapFilters(size_t index1, size_t index2);
	size_t GetFilterCount() const;
	bool RemoveFilter(size_t index);
	auto GetFilters() const {
		return m_Filters | std::views::transform([](auto& f) { return f.get(); });
	}

private:
	std::vector<std::unique_ptr<FilterBase>> m_Filters;
	FilterResult m_DefaultResult{ FilterResult::Include };
};
