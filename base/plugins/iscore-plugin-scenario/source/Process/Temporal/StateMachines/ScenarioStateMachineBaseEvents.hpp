#pragma once
#include "ScenarioPoint.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <iscore/tools/Clamp.hpp>

class TimeNodeModel;
class EventModel;
class ConstraintModel;
class SlotModel;

template<>
struct PositionedEvent<ScenarioPoint> : public QEvent
{
        PositionedEvent(
                const ScenarioPoint& pt,
                QEvent::Type type):
            QEvent{type},
            point(pt)
        {
            // Here we artificially prevent to move over the header of the box
            // so that the elements won't disappear in the void.
            point.y = clamp(point.y, 0.004, 0.99);
        }

        ScenarioPoint point;
};

// We avoid virtual inheritance (with Numbered event);
// this replicates a tiny bit of code.
template<int N>
struct PositionedScenarioEvent : public PositionedEvent<ScenarioPoint>
{
        static constexpr const int user_type = N;
        PositionedScenarioEvent(
                const ScenarioPoint& pt):
            PositionedEvent<ScenarioPoint>{pt, QEvent::Type(QEvent::User + N)}
        {
        }
};

template<typename Element, int N>
struct PositionedWithId_ScenarioEvent : public PositionedScenarioEvent<N>
{
        PositionedWithId_ScenarioEvent(
                const id_type<Element>& tn_id,
                const ScenarioPoint& sp):
            PositionedScenarioEvent<N>{sp},
            id{tn_id}
        {
        }

        id_type<Element> id;
};


////////////
// Events
enum ScenarioElement {
    Nothing, TimeNode, Event, Constraint, SlotOverlay_e, SlotHandle_e
};

using ClickOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier_tagme::Click>;
using ClickOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier_tagme::Click>;
using ClickOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier_tagme::Click>;
using ClickOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier_tagme::Click>;

using ClickOnSlotOverlay_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier_tagme::Click>;
using ClickOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier_tagme::Click>;


using MoveOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier_tagme::Move>;
using MoveOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier_tagme::Move>;
using MoveOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier_tagme::Move>;
using MoveOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier_tagme::Move>;

using MoveOnSlot_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier_tagme::Move>;
using MoveOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier_tagme::Move>;


using ReleaseOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier_tagme::Release>;
using ReleaseOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier_tagme::Release>;
using ReleaseOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier_tagme::Release>;
using ReleaseOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier_tagme::Release>;

using ReleaseOnSlot_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier_tagme::Release>;
using ReleaseOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier_tagme::Release>;
