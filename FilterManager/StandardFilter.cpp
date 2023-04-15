#include "pch.h"
#include "StandardFilter.h"
#include <algorithm>

FilterResult StandardFilter::Eval(FilterValue const& value) {
    if (value.GetType() != GetValue().GetType()) {
        //
        // if not the same type, skip
        //
        return FilterResult::Passthrough;
    }

    switch (GetValue().GetType()) {
        case FilterValueType::Integer: return Compare(GetValue().GetInteger(), value.GetInteger());
        case FilterValueType::AnsiString: return Compare(GetValue().GetAnsiString(), value.GetAnsiString());
        case FilterValueType::UnicodeString: return Compare(GetValue().GetUnicodeString(), value.GetUnicodeString());
    }

    return FilterResult::Passthrough;
}

FilterResult StandardFilter::Compare(int64_t left, int64_t right) const {
    switch (GetCompareType()) {
        case CompareType::Equal: return left == right ? GetResult() : FilterResult::Passthrough;
        case CompareType::NotEqual: return left != right ? GetResult() : FilterResult::Passthrough;
        case CompareType::LessThan: return left < right ? GetResult() : FilterResult::Passthrough;
        case CompareType::LessThanOrEqual: return left <= right ? GetResult() : FilterResult::Passthrough;
        case CompareType::GreaterThan: return left > right ? GetResult() : FilterResult::Passthrough;
        case CompareType::GreaterThanOrEqual: return left > right ? GetResult() : FilterResult::Passthrough;
    }
    return FilterResult::Passthrough;
}

FilterResult StandardFilter::Compare(std::wstring const& left, std::wstring const& right) const {
    switch (GetCompareType()) {
        case CompareType::Equal: return _wcsicmp(left.c_str(), right.c_str()) == 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::NotEqual: return _wcsicmp(left.c_str(), right.c_str()) != 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::LessThan: return _wcsicmp(left.c_str(), right.c_str()) < 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::LessThanOrEqual: return _wcsicmp(left.c_str(), right.c_str()) <= 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::GreaterThan: return _wcsicmp(left.c_str(), right.c_str()) > 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::GreaterThanOrEqual: return _wcsicmp(left.c_str(), right.c_str()) >= 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::StartsWith: return _wcsnicmp(left.c_str(), right.c_str(), right.length()) ? GetResult() : FilterResult::Passthrough;
        case CompareType::EndsWith: return left.length() >= right.length() && _wcsicmp(left.c_str() - right.length(), right.c_str()) == 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::Contains:
        case CompareType::NotContains:
            auto sleft(left), sright(right);
            std::transform(sleft.begin(), sleft.end(), sleft.begin(), ::toupper);
            std::transform(sright.begin(), sright.end(), sright.begin(), ::toupper);
            switch (GetCompareType()) {
                case CompareType::Contains: return wcsstr(sleft.c_str(), sright.c_str()) ? GetResult() : FilterResult::Passthrough;
                case CompareType::NotContains: return wcsstr(sleft.c_str(), sright.c_str()) == nullptr ? GetResult() : FilterResult::Passthrough;
            }
    }
    return FilterResult::Passthrough;
}

FilterResult StandardFilter::Compare(std::string const& left, std::string const& right) const {
    switch (GetCompareType()) {
        case CompareType::Equal: return _stricmp(left.c_str(), right.c_str()) == 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::NotEqual: return _stricmp(left.c_str(), right.c_str()) != 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::LessThan: return _stricmp(left.c_str(), right.c_str()) < 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::LessThanOrEqual: return _stricmp(left.c_str(), right.c_str()) <= 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::GreaterThan: return _stricmp(left.c_str(), right.c_str()) > 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::GreaterThanOrEqual: return _stricmp(left.c_str(), right.c_str()) >= 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::StartsWith: return _strnicmp(left.c_str(), right.c_str(), right.length()) ? GetResult() : FilterResult::Passthrough;
        case CompareType::EndsWith: return left.length() >= right.length() && _stricmp(left.c_str() - right.length(), right.c_str()) == 0 ? GetResult() : FilterResult::Passthrough;
        case CompareType::Contains:
        case CompareType::NotContains:
            auto sleft(left), sright(right);
            std::transform(sleft.begin(), sleft.end(), sleft.begin(), ::toupper);
            std::transform(sright.begin(), sright.end(), sright.begin(), ::toupper);
            switch (GetCompareType()) {
                case CompareType::Contains: return strstr(sleft.c_str(), sright.c_str()) ? GetResult() : FilterResult::Passthrough;
                case CompareType::NotContains: return strstr(sleft.c_str(), sright.c_str()) == nullptr ? GetResult() : FilterResult::Passthrough;
            }
    }
    return FilterResult::Passthrough;
}

std::unique_ptr<FilterBase> StandardFilter::Clone() const {
    auto filter = std::make_unique<StandardFilter>();
    return filter;
}
