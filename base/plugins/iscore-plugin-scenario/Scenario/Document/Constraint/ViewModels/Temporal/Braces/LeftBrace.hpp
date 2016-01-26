#pragma once

#include "ConstraintBrace.hpp"

namespace Scenario
{

class ISCORE_PLUGIN_SCENARIO_EXPORT LeftBraceView : public ConstraintBrace
{
    public:
        LeftBraceView(const TemporalConstraintView& parentCstr, QGraphicsItem* parent);

        static constexpr int static_type()
        { return QGraphicsItem::UserType + 7; }
        int type() const override
        { return static_type(); }

};

}
