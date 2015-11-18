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

namespace iscore
{
template<>
struct PositionedEvent<Scenario::Point> : public QEvent
{
        PositionedEvent(
                const Scenario::Point& pt,
                QEvent::Type type):
            QEvent{type},
            point(pt)
        {
            // Here we artificially prevent to move over the header of the rack
            // so that the elements won't disappear in the void.
            point.y = clamp(point.y, 0.004, 0.99);
        }

        Scenario::Point point;
};
}

// We avoid virtual inheritance (with Numbered event);
// this replicates a tiny bit of code.
template<int N>
struct PositionedScenarioEvent : public iscore::PositionedEvent<Scenario::Point>
{
        static constexpr const int user_type = N;
        PositionedScenarioEvent(
                const Scenario::Point& pt):
            PositionedEvent<Scenario::Point>{pt, QEvent::Type(QEvent::User + N)}
        {
        }
};

template<typename Element, int N>
struct PositionedWithId_ScenarioEvent final : public PositionedScenarioEvent<N>
{
        PositionedWithId_ScenarioEvent(
                const Id<Element>& tn_id,
                const Scenario::Point& sp):
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
using ClickOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + iscore::Modifier::Click_tag::value>;
using ClickOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + iscore::Modifier::Click_tag::value>;
using ClickOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + iscore::Modifier::Click_tag::value>;
using ClickOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + iscore::Modifier::Click_tag::value>;
using ClickOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + iscore::Modifier::Click_tag::value>;

using ClickOnSlotOverlay_Event = iscore::NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + iscore::Modifier::Click_tag::value>;
using ClickOnSlotHandle_Event = iscore::NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + iscore::Modifier::Click_tag::value>;

using ClickOnTrigger_Event = iscore::NumberedWithPath_Event<TriggerModel, ScenarioElement::Trigger + iscore::Modifier::Click_tag::value>;

/* move on */
using MoveOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + iscore::Modifier::Move_tag::value>;
using MoveOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + iscore::Modifier::Move_tag::value>;
using MoveOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + iscore::Modifier::Move_tag::value>;
using MoveOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + iscore::Modifier::Move_tag::value>;
using MoveOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + iscore::Modifier::Move_tag::value>;

using MoveOnSlot_Event = iscore::NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + iscore::Modifier::Move_tag::value>;
using MoveOnSlotHandle_Event = iscore::NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + iscore::Modifier::Move_tag::value>;

using MoveOnTrigger_Event = iscore::NumberedWithPath_Event<TriggerModel, ScenarioElement::Trigger + iscore::Modifier::Move_tag::value>;

/* release on */
using ReleaseOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + iscore::Modifier::Release_tag::value>;
using ReleaseOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + iscore::Modifier::Release_tag::value>;
using ReleaseOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + iscore::Modifier::Release_tag::value>;
using ReleaseOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + iscore::Modifier::Release_tag::value>;
using ReleaseOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ScenarioElement::State + iscore::Modifier::Release_tag::value>;

using ReleaseOnSlot_Event = iscore::NumberedWithPath_Event<SlotModel, ScenarioElement::SlotOverlay_e + iscore::Modifier::Release_tag::value>;
using ReleaseOnSlotHandle_Event = iscore::NumberedWithPath_Event<SlotModel, ScenarioElement::SlotHandle_e + iscore::Modifier::Release_tag::value>;

using ReleaseOnTrigger_Event = iscore::NumberedWithPath_Event<TriggerModel, ScenarioElement::Trigger + iscore::Modifier::Release_tag::value>;

template<int N>
QString debug_StateMachineIDs()
{
    QString txt;

    auto object = static_cast<ScenarioElement>(N % 10);
    auto modifier = static_cast<iscore::Modifier_tagme>((N - object) % 1000 / 100);
    switch(modifier)
    {
        case iscore::Modifier_tagme::Click:
            txt += "Click on";
            break;
        case iscore::Modifier_tagme::Move:
            txt += "Move on";
            break;
        case iscore::Modifier_tagme::Release:
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
