#include "pch.h"
#include "FilterManager.h"

FilterResult FilterManager::Eval(FilterValue const& value) {
	for (auto& filter : m_Filters) {
		if (!filter->IsEnabled())
			continue;
		auto result = filter->Eval(value);
		if (result != FilterResult::Passthrough)
			return result;
	}
	return m_DefaultResult;
}

FilterManager FilterManager::Clone() const {
	FilterManager mgr;
	for (auto& filter : m_Filters)
		mgr.AddFilter(filter->Clone());
	mgr.SetDefaultResult(m_DefaultResult);
	return mgr;
}

void FilterManager::AddFilter(std::unique_ptr<FilterBase> filter) {
	m_Filters.push_back(std::move(filter));
}

void FilterManager::SetDefaultResult(FilterResult result) {
	m_DefaultResult = result;
}

FilterResult FilterManager::GetDefaultReesult() const {
	return m_DefaultResult;
}

void FilterManager::Clear() {
	m_Filters.clear();
}

bool FilterManager::SwapFilters(size_t index1, size_t index2) {
	if(index1 >= m_Filters.size() || index2 >= m_Filters.size() || index1 == index2)
		return false;
	std::swap(m_Filters[index1], m_Filters[index2]);
	return true;
}

size_t FilterManager::GetFilterCount() const {
	return m_Filters.size();
}

bool FilterManager::RemoveFilter(size_t index) {
	if(index >= m_Filters.size())
		return false;
	m_Filters.erase(m_Filters.begin() + index);
	return true;
}
