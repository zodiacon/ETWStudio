#pragma once

#include <variant>
#include <any>

enum class FilterValueType {
	Integer,
	AnsiString,
	UnicodeString,
	Range,
	Custom,
	None = -1,
};

struct NumericRange {
	int64_t Min;
	int64_t Max;
};

class FilterValue final {
public:
	FilterValue() = default;
	FilterValue(int64_t value);
	FilterValue(std::string value);
	FilterValue(std::wstring value);
	FilterValue(NumericRange const& range);
	template<typename T>
	FilterValue(T const& value) : m_Value(std::any(value)) {}

	FilterValueType GetType() const;
	bool HasValue() const;

	std::wstring const& GetUnicodeString() const;
	std::string const& GetAnsiString() const;
	int64_t GetInteger() const;
	NumericRange const& GetRange() const;
	template<typename T>
	T GetAny() const {
		return std::any_cast<T>(std::get<std::any>(m_Value));
	}

private:
	std::variant<int64_t, std::string, std::wstring, NumericRange, std::any> m_Value;
};

