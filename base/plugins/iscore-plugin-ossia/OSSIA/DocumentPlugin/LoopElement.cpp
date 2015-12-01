#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <algorithm>
#include <vector>

#include "Editor/Loop.h"
#include "Editor/TimeValue.h"
#include "Editor/State.h"
#include "Loop/LoopProcessModel.hpp"
#include "LoopElement.hpp"
#include <OSSIA/DocumentPlugin/ConstraintElement.hpp>
#include <OSSIA/DocumentPlugin/EventElement.hpp>
#include <OSSIA/DocumentPlugin/ProcessElement.hpp>
#include <OSSIA/DocumentPlugin/TimeNodeElement.hpp>
#include <OSSIA/DocumentPlugin/StateElement.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>

class Process;
class QObject;
namespace OSSIA {
class StateElement;
class TimeProcess;
}  // namespace OSSIA
#include <iscore/tools/SettableIdentifier.hpp>

RecreateOnPlay::LoopElement::LoopElement(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        Loop::ProcessModel& element,
        QObject* parent):
    ProcessElement{parentConstraint, parent},
    m_iscore_loop{element},
    m_deviceList{iscore::IDocument::documentFromObject(element)->context().plugin<DeviceDocumentPlugin>().list()}
{
    OSSIA::TimeValue main_duration(iscore::convert::time(element.constraint().duration.defaultDuration()));

    m_ossia_loop = OSSIA::Loop::create(main_duration,
                                       [] (const OSSIA::TimeValue&, const OSSIA::TimeValue&, std::shared_ptr<OSSIA::StateElement>) {},
    [] (OSSIA::TimeEvent::Status) {},
    [] (OSSIA::TimeEvent::Status) {}
    );

    // TODO also states in BasEelement
    // TODO put graphical settings somewhere.

    auto startTN = m_ossia_loop->getPatternStartTimeNode();
    auto endTN = m_ossia_loop->getPatternEndTimeNode();
    auto startEV = *startTN->timeEvents().begin();
    auto endEV = *endTN->timeEvents().begin();


    m_ossia_startTimeNode = new TimeNodeElement{startTN, element.startTimeNode(),  m_deviceList, this};
    m_ossia_endTimeNode = new TimeNodeElement{endTN, element.endTimeNode(), m_deviceList, this};

    m_ossia_startEvent = new EventElement{startEV, element.startEvent(), m_deviceList, this};
    m_ossia_endEvent = new EventElement{endEV, element.endEvent(), m_deviceList, this};

    m_ossia_startState = new StateElement{
            element.startState(),
            iscore::convert::state(element.startState(), m_deviceList),
            m_deviceList,
            this};
    m_ossia_endState = new StateElement{
            element.endState(),
            iscore::convert::state(element.endState(), m_deviceList),
            m_deviceList,
            this};

    startEV->getState()->stateElements().push_back(m_ossia_startState->OSSIAState());
    endEV->getState()->stateElements().push_back(m_ossia_endState->OSSIAState());
    m_ossia_constraint = new ConstraintElement{m_ossia_loop->getPatternTimeConstraint(), element.constraint(), this};
}

RecreateOnPlay::LoopElement::~LoopElement()
{
}

std::shared_ptr<OSSIA::TimeProcess> RecreateOnPlay::LoopElement::OSSIAProcess() const
{ return m_ossia_loop; }

std::shared_ptr<OSSIA::Loop> RecreateOnPlay::LoopElement::scenario() const
{ return m_ossia_loop; }

Process&RecreateOnPlay::LoopElement::iscoreProcess() const
{ return m_iscore_loop; }

void RecreateOnPlay::LoopElement::stop()
{
    ProcessElement::stop();
    m_iscore_loop.constraint().duration.setPlayPercentage(0);
}

void RecreateOnPlay::LoopElement::startConstraintExecution(const Id<ConstraintModel>&)
{
    m_ossia_constraint->executionStarted();
}

void RecreateOnPlay::LoopElement::stopConstraintExecution(const Id<ConstraintModel>&)
{
    m_ossia_constraint->executionStopped();
}
