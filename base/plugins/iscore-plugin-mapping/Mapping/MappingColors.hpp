#pragma once
#include <Curve/CurveStyle.hpp>

namespace Mapping
{
class MappingColors
{
    public:
        MappingColors();

        const auto& style() const
        { return m_style; }

    private:
        Curve::Style m_style;
};
}
