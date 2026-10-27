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

    //
    // left: the evaluated value, right: the filter's value (e.g. "value < 100")
    //
    switch (GetValue().GetType()) {
        case FilterValueType::Integer: return Compare(value.GetInteger(), GetValue().GetInteger());
        case FilterValueType::AnsiString: return Compare(value.GetAnsiString(), GetValue().GetAnsiString());
        case FilterValueType::UnicodeString: return Compare(value.GetUnicodeString(), GetValue().GetUnicodeString());
    }

    return FilterResult::Passthrough;
}

FilterResult StandardFilter::Compare(int64_t left, int64_t right) const {
    bool match = false;
    switch (GetCompareType()) {
        case CompareType::Equal: match = left == right; break;
        case CompareType::NotEqual: match = left != right; break;
        case CompareType::LessThan: match = left < right; break;
        case CompareType::LessThanOrEqual: match = left <= right; break;
        case CompareType::GreaterThan: match = left > right; break;
        case CompareType::GreaterThanOrEqual: match = left >= right; break;
    }
    return match ? GetResult() : FilterResult::Passthrough;
}

namespace {
    template<typename Char>
    int CompareNoCase(Char const* s1, Char const* s2, size_t count = size_t(-1)) {
        if constexpr (sizeof(Char) == sizeof(char))
            return count == size_t(-1) ? _stricmp(s1, s2) : _strnicmp(s1, s2, count);
        else
            return count == size_t(-1) ? _wcsicmp(s1, s2) : _wcsnicmp(s1, s2, count);
    }

    template<typename String>
    bool CompareStrings(CompareType type, String const& left, String const& right) {
        switch (type) {
            case CompareType::Equal: return CompareNoCase(left.c_str(), right.c_str()) == 0;
            case CompareType::NotEqual: return CompareNoCase(left.c_str(), right.c_str()) != 0;
            case CompareType::LessThan: return CompareNoCase(left.c_str(), right.c_str()) < 0;
            case CompareType::LessThanOrEqual: return CompareNoCase(left.c_str(), right.c_str()) <= 0;
            case CompareType::GreaterThan: return CompareNoCase(left.c_str(), right.c_str()) > 0;
            case CompareType::GreaterThanOrEqual: return CompareNoCase(left.c_str(), right.c_str()) >= 0;
            case CompareType::StartsWith: return CompareNoCase(left.c_str(), right.c_str(), right.length()) == 0;
            case CompareType::EndsWith:
                return left.length() >= right.length() && CompareNoCase(left.c_str() + left.length() - right.length(), right.c_str()) == 0;
            case CompareType::Contains:
            case CompareType::NotContains:
            {
                auto upper = [](String s) {
                    if constexpr (sizeof(s[0]) == sizeof(char))
                        std::transform(s.begin(), s.end(), s.begin(), [](char c) { return (char)::toupper((unsigned char)c); });
                    else
                        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)::towupper(c); });
                    return s;
                };
                bool contains = upper(left).find(upper(right)) != String::npos;
                return type == CompareType::Contains ? contains : !contains;
            }
        }
        return false;
    }
}

FilterResult StandardFilter::Compare(std::wstring const& left, std::wstring const& right) const {
    return CompareStrings(GetCompareType(), left, right) ? GetResult() : FilterResult::Passthrough;
}

FilterResult StandardFilter::Compare(std::string const& left, std::string const& right) const {
    return CompareStrings(GetCompareType(), left, right) ? GetResult() : FilterResult::Passthrough;
}

std::unique_ptr<FilterBase> StandardFilter::Clone() const {
    auto filter = std::make_unique<StandardFilter>();
    CopyStateTo(*filter);
    return filter;
}
