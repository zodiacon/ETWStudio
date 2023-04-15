#include "pch.h"
#include "PropertyFilter.h"
#include <EventData.h>

PropertyFilter::PropertyFilter(std::wstring propertyName) :
    m_PropertyName(std::move(propertyName)) {
}

FilterResult PropertyFilter::Eval(FilterValue const& value) {
    assert(value.GetType() == FilterValueType::Custom);
    auto evt = value.GetAny<EventData*>();
    auto prop = evt->GetProperty(m_PropertyName.c_str());
    if (!prop)
        return FilterResult::Passthrough;

    auto type = prop->Info.nonStructType.InType;
    int64_t ivalue = 0;
    std::string svalue;
    std::wstring usvalue;
    bool novalue = false;
    switch (type) {
        case TDH_INTYPE_INT8:
        case TDH_INTYPE_UINT8:
            ivalue = *prop->GetData();
            break;
        case TDH_INTYPE_FILETIME:
        case TDH_INTYPE_UINT64:
        case TDH_INTYPE_INT64:
            ivalue = *(int64_t*)prop->GetData();
            break;

        case TDH_INTYPE_BOOLEAN:
        case TDH_INTYPE_INT32:
        case TDH_INTYPE_UINT32:
            ivalue = *(int32_t*)prop->GetData();
            break;

        case TDH_INTYPE_INT16:
        case TDH_INTYPE_UINT16:
            ivalue = *(int16_t*)prop->GetData();
            break;

        case TDH_INTYPE_ANSISTRING:
            svalue = (PCSTR)prop->GetData();
            break;

        case TDH_INTYPE_UNICODESTRING:
            usvalue = (PCWSTR)prop->GetData();
            break;

        default:
            novalue = true;
            break;
    }

    if (novalue)
        return FilterResult::Passthrough;

    if (!usvalue.empty())
        return StandardFilter::Eval(usvalue);

    if (!svalue.empty())
        return StandardFilter::Eval(svalue);

    return StandardFilter::Eval(ivalue);
}

std::unique_ptr<FilterBase> PropertyFilter::Clone() const {
    auto filter = std::make_unique<PropertyFilter>(m_PropertyName);
    return filter;
}
