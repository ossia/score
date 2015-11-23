#pragma once
#include <Curve/CurveStyle.hpp>

class AutomationColors
{
    public:
        AutomationColors();

        const auto& style() const
        { return m_style; }

    private:
        Curve::Style m_style;
};
