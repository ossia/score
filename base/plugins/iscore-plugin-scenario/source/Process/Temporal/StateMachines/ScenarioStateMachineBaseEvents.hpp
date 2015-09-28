#pragma once
#include "ScenarioPoint.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <iscore/tools/Clamp.hpp>

class TimeNodeModel;
class EventModel;
class ConstraintModel;
class StateModel;
class SlotModel;
class TriggerModel;

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
                const Id<Element>& tn_id,
                const ScenarioPoint& sp):
            PositionedScenarioEvent<N>{sp},
            id{tn_id}
        {
        }

        Id<Element> id;
};

////////////
// Events
enum ScenarioElement {
    Nothing, TimeNode, Event, Constraint, State, SlotOverlay_e, SlotHandle_e, Trigger
};

/* click */
using ClickOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Click_tag::value>;
using ClickOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Click_tag::value>;
using ClickOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Click_tag::value>;
using ClickOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Click_tag::value>;
using ClickOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Click_tag::value>;

using ClickOnSlotOverlay_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier::Click_tag::value>;
using ClickOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier::Click_tag::value>;

using ClickOnTrigger_Event = NumberedWithPath_Event<TriggerModel, ScenarioElement::Trigger + Modifier::Click_tag::value>;

/* move on */
using MoveOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Move_tag::value>;
using MoveOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Move_tag::value>;
using MoveOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Move_tag::value>;
using MoveOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Move_tag::value>;
using MoveOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Move_tag::value>;

using MoveOnSlot_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier::Move_tag::value>;
using MoveOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier::Move_tag::value>;

using MoveOnTrigger_Event = NumberedWithPath_Event<TriggerModel, ScenarioElement::Trigger + Modifier::Move_tag::value>;

/* release on */
using ReleaseOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Release_tag::value>;
using ReleaseOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Release_tag::value>;
using ReleaseOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Release_tag::value>;
using ReleaseOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Release_tag::value>;
using ReleaseOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + Modifier::Release_tag::value>;

using ReleaseOnSlot_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + Modifier::Release_tag::value>;
using ReleaseOnSlotHandle_Event = NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + Modifier::Release_tag::value>;

using ReleaseOnTrigger_Event = NumberedWithPath_Event<TriggerModel, ScenarioElement::Trigger + Modifier::Release_tag::value>;

template<int N>
QString debug_StateMachineIDs()
{
    QString txt;

    auto object = static_cast<ScenarioElement>(N % 10);
    auto modifier = static_cast<Modifier_tagme>((N - object) % 1000 / 100);
    switch(modifier)
    {
        case Modifier_tagme::Click:
            txt += "Click on";
            break;
        case Modifier_tagme::Move:
            txt += "Move on";
            break;
        case Modifier_tagme::Release:
            txt += "Release on";
            break;
    }

    switch(object)
    {
        case ScenarioElement::Nothing:
            txt += "nothing";
            break;
        case ScenarioElement::TimeNode:
            txt += "TimeNode";
            break;
        case ScenarioElement::Event:
            txt += "Event";
            break;
        case ScenarioElement::Constraint:
            txt += "Constraint";
            break;
        case ScenarioElement::State:
            txt += "State";
            break;
        case ScenarioElement::SlotOverlay_e:
            txt += "SlotOverlay_e";
            break;
        case ScenarioElement::SlotHandle_e:
            txt += "SlotHandle_e";
            break;
        case ScenarioElement::Trigger:
            txt += "Trigger";
            break;
    }

    return txt;
}
