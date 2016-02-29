#pragma once

#include "ConstraintBrace.hpp"
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
namespace Scenario
{

class ISCORE_PLUGIN_SCENARIO_EXPORT LeftBraceView : public ConstraintBrace
{
    public:
        LeftBraceView(const TemporalConstraintView& parentCstr, QGraphicsItem* parent);

        static constexpr int static_type()
        { return QGraphicsItem::UserType + ItemType::LeftBrace; }
        int type() const override
        { return static_type(); }

};

}
