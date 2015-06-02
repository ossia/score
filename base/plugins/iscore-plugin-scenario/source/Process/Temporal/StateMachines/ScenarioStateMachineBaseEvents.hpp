#pragma once
#include "ScenarioPoint.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>

#include <iscore/tools/Clamp.hpp>

class TimeNodeModel;
class EventModel;
class ConstraintModel;
class DeckModel;

struct PositionedScenarioEventBase : public QEvent
{
        PositionedScenarioEventBase(const ScenarioPoint& pt, QEvent::Type type):
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
struct PositionedScenarioEvent : public PositionedScenarioEventBase
{
        static constexpr const int user_type = N;
        PositionedScenarioEvent(
                const ScenarioPoint& pt):
            PositionedScenarioEventBase{pt, QEvent::Type(QEvent::User + N)}
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
    Nothing, TimeNode, Event, Constraint, DeckOverlay_e, DeckHandle_e
};

using ClickOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Click>;
using ClickOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Click>;
using ClickOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Click>;
using ClickOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Click>;

using ClickOnDeckOverlay_Event = NumberedWithPath_Event<DeckModel, ScenarioElement::DeckOverlay_e + Modifier::Click>;
using ClickOnDeckHandle_Event = NumberedWithPath_Event<DeckModel, ScenarioElement::DeckHandle_e + Modifier::Click>;


using MoveOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Move>;
using MoveOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Move>;
using MoveOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Move>;
using MoveOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Move>;

using MoveOnDeck_Event = NumberedWithPath_Event<DeckModel, ScenarioElement::DeckOverlay_e + Modifier::Move>;
using MoveOnDeckHandle_Event = NumberedWithPath_Event<DeckModel, ScenarioElement::DeckHandle_e + Modifier::Move>;


using ReleaseOnNothing_Event = PositionedScenarioEvent<ScenarioElement::Nothing + Modifier::Release>;
using ReleaseOnTimeNode_Event = PositionedWithId_ScenarioEvent<TimeNodeModel, ScenarioElement::TimeNode + Modifier::Release>;
using ReleaseOnEvent_Event = PositionedWithId_ScenarioEvent<EventModel, ScenarioElement::Event + Modifier::Release>;
using ReleaseOnConstraint_Event = PositionedWithId_ScenarioEvent<ConstraintModel, ScenarioElement::Constraint + Modifier::Release>;

using ReleaseOnDeck_Event = NumberedWithPath_Event<DeckModel, ScenarioElement::DeckOverlay_e + Modifier::Release>;
using ReleaseOnDeckHandle_Event = NumberedWithPath_Event<DeckModel, ScenarioElement::DeckHandle_e + Modifier::Release>;
