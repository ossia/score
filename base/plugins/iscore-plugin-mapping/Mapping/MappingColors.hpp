#pragma once
#include <Curve/CurveStyle.hpp>

// RENAMEME
class MappingColors
{
    public:
        MappingColors();

        const auto& style() const
        { return m_style; }

    private:
        CurveStyle m_style;
};
