#include <QPointer>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <algorithm>

#include "DisplayedElementsModel.hpp"
#include <ossia/detail/algorithms.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <iscore/selection/Selection.hpp>

namespace Scenario
{
void DisplayedElementsModel::setSelection(const Selection& s)
{
  ossia::for_each_in_tuple(elements(), [&](auto elt) {
    elt->selection.set(s.contains(elt.data())); // OPTIMIZEME
  });
}

void DisplayedElementsModel::setDisplayedElements(
    DisplayedElementsContainer&& elts)
{
  m_elements = std::move(elts);
  m_initialized = true;
}

ConstraintModel& DisplayedElementsModel::constraint() const
{
  return *m_elements.constraint;
}

const TimeNodeModel& DisplayedElementsModel::startTimeNode() const
{
  return *m_elements.startNode;
}

const TimeNodeModel& DisplayedElementsModel::endTimeNode() const
{
  return *m_elements.endNode;
}

const EventModel& DisplayedElementsModel::startEvent() const
{
  return *m_elements.startEvent;
}

const EventModel& DisplayedElementsModel::endEvent() const
{
  return *m_elements.endEvent;
}

const StateModel& DisplayedElementsModel::startState() const
{
  return *m_elements.startState;
}

const StateModel& DisplayedElementsModel::endState() const
{
  return *m_elements.endState;
}
}
