#pragma once

#include "ConstraintBrace.hpp"

namespace Scenario
{

class ISCORE_PLUGIN_SCENARIO_EXPORT RightBraceView : public ConstraintBrace
{
    public:
        RightBraceView(const TemporalConstraintView& parentCstr, QGraphicsItem* parent);

        static constexpr int static_type()
        { return QGraphicsItem::UserType + 8; }
        int type() const override
        { return static_type(); }

};

}
