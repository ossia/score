#pragma once

#include "ConstraintBrace.hpp"
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
namespace Scenario
{

class ISCORE_PLUGIN_SCENARIO_EXPORT RightBraceView : public ConstraintBrace
{
    public:
        RightBraceView(const TemporalConstraintView& parentCstr, QGraphicsItem* parent);

        static constexpr int static_type()
        { return QGraphicsItem::UserType + ItemType::RightBrace; }
        int type() const override
        { return static_type(); }

};

}
