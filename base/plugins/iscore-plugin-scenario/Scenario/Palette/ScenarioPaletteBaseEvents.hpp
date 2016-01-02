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

namespace Scenario
{

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

static const constexpr int ClickOnNothing     = ScenarioElement::Nothing       + iscore::Modifier::Click_tag::value;
static const constexpr int ClickOnTimeNode    = ScenarioElement::TimeNode      + iscore::Modifier::Click_tag::value;
static const constexpr int ClickOnEvent       = ScenarioElement::Event         + iscore::Modifier::Click_tag::value;
static const constexpr int ClickOnConstraint  = ScenarioElement::Constraint    + iscore::Modifier::Click_tag::value;
static const constexpr int ClickOnState       = ScenarioElement::State         + iscore::Modifier::Click_tag::value;
static const constexpr int ClickOnSlotOverlay = ScenarioElement::SlotOverlay_e + iscore::Modifier::Click_tag::value;
static const constexpr int ClickOnSlotHandle  = ScenarioElement::SlotHandle_e  + iscore::Modifier::Click_tag::value;
static const constexpr int ClickOnTrigger     = ScenarioElement::Trigger       + iscore::Modifier::Click_tag::value;

static const constexpr int MoveOnNothing     = ScenarioElement::Nothing       + iscore::Modifier::Move_tag::value;
static const constexpr int MoveOnTimeNode    = ScenarioElement::TimeNode      + iscore::Modifier::Move_tag::value;
static const constexpr int MoveOnEvent       = ScenarioElement::Event         + iscore::Modifier::Move_tag::value;
static const constexpr int MoveOnConstraint  = ScenarioElement::Constraint    + iscore::Modifier::Move_tag::value;
static const constexpr int MoveOnState       = ScenarioElement::State         + iscore::Modifier::Move_tag::value;
static const constexpr int MoveOnSlotOverlay = ScenarioElement::SlotOverlay_e + iscore::Modifier::Move_tag::value;
static const constexpr int MoveOnSlotHandle  = ScenarioElement::SlotHandle_e  + iscore::Modifier::Move_tag::value;
static const constexpr int MoveOnTrigger     = ScenarioElement::Trigger       + iscore::Modifier::Move_tag::value;

static const constexpr int ReleaseOnNothing     = ScenarioElement::Nothing       + iscore::Modifier::Release_tag::value;
static const constexpr int ReleaseOnTimeNode    = ScenarioElement::TimeNode      + iscore::Modifier::Release_tag::value;
static const constexpr int ReleaseOnEvent       = ScenarioElement::Event         + iscore::Modifier::Release_tag::value;
static const constexpr int ReleaseOnConstraint  = ScenarioElement::Constraint    + iscore::Modifier::Release_tag::value;
static const constexpr int ReleaseOnState       = ScenarioElement::State         + iscore::Modifier::Release_tag::value;
static const constexpr int ReleaseOnSlotOverlay = ScenarioElement::SlotOverlay_e + iscore::Modifier::Release_tag::value;
static const constexpr int ReleaseOnSlotHandle  = ScenarioElement::SlotHandle_e  + iscore::Modifier::Release_tag::value;
static const constexpr int ReleaseOnTrigger     = ScenarioElement::Trigger       + iscore::Modifier::Release_tag::value;

/* click */
using ClickOnNothing_Event = PositionedScenarioEvent<ClickOnNothing>;
using ClickOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ClickOnTimeNode>;
using ClickOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ClickOnEvent>;
using ClickOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ClickOnConstraint>;
using ClickOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ClickOnState>;
using ClickOnSlotOverlay_Event = iscore::NumberedWithPath_Event<SlotModel, ClickOnSlotOverlay>;
using ClickOnSlotHandle_Event = iscore::NumberedWithPath_Event<SlotModel, ClickOnSlotHandle>;
using ClickOnTrigger_Event = iscore::NumberedWithPath_Event<TriggerModel, ClickOnTrigger>;

/* move on */
using MoveOnNothing_Event = PositionedScenarioEvent<MoveOnNothing>;
using MoveOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, MoveOnTimeNode>;
using MoveOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, MoveOnEvent>;
using MoveOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, MoveOnConstraint>;
using MoveOnState_Event = PositionedWithId_ScenarioEvent<StateModel, MoveOnState>;
using MoveOnSlot_Event = iscore::NumberedWithPath_Event<SlotModel, MoveOnSlotOverlay>;
using MoveOnSlotHandle_Event = iscore::NumberedWithPath_Event<SlotModel, MoveOnSlotHandle>;
using MoveOnTrigger_Event = iscore::NumberedWithPath_Event<TriggerModel, MoveOnTrigger>;

/* release on */
using ReleaseOnNothing_Event = PositionedScenarioEvent<ReleaseOnNothing>;
using ReleaseOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ReleaseOnTimeNode>;
using ReleaseOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ReleaseOnEvent>;
using ReleaseOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ReleaseOnConstraint>;
using ReleaseOnState_Event = PositionedWithId_ScenarioEvent<StateModel, ReleaseOnState>;
using ReleaseOnSlot_Event = iscore::NumberedWithPath_Event<SlotModel, ReleaseOnSlotOverlay>;
using ReleaseOnSlotHandle_Event = iscore::NumberedWithPath_Event<SlotModel, ReleaseOnSlotHandle>;
using ReleaseOnTrigger_Event = iscore::NumberedWithPath_Event<TriggerModel, ReleaseOnTrigger>;

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
}
