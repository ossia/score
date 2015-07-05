#pragma once
#include "ScenarioPoint.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <iscore/tools/Clamp.hpp>

class TimeNodeModel;
class EventModel;
class ConstraintModel;
class StateModel;
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
            // Here we artificially prevent to move over the header of the rack
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
    Nothing, TimeNode, Event, Constraint, State, SlotOverlay_e, SlotHandle_e
};

using ClickOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Click_tag::value>;
using ClickOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Click_tag::value>;
using ClickOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Click_tag::value>;
using ClickOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Click_tag::value>;
using ClickOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Click_tag::value>;

using ClickOnSlotOverlay_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier::Click_tag::value>;
using ClickOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier::Click_tag::value>;


using MoveOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Move_tag::value>;
using MoveOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Move_tag::value>;
using MoveOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Move_tag::value>;
using MoveOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Move_tag::value>;
using MoveOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Move_tag::value>;

using MoveOnSlot_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier::Move_tag::value>;
using MoveOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier::Move_tag::value>;


using ReleaseOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Release_tag::value>;
using ReleaseOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Release_tag::value>;
using ReleaseOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Release_tag::value>;
using ReleaseOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Release_tag::value>;
using ReleaseOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Release_tag::value>;

using ReleaseOnSlot_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier::Release_tag::value>;
using ReleaseOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier::Release_tag::value>;
