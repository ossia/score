#include "LoopElement.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <API/Headers/Editor/TimeConstraint.h>
#include <API/Headers/Editor/TimeEvent.h>
#include <API/Headers/Editor/TimeNode.h>
#include <API/Headers/Editor/State.h>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore2OSSIA.hpp>
#include <OSSIA2iscore.hpp>

RecreateOnPlay::LoopElement::LoopElement(
        RecreateOnPlay::ConstraintElement& parentConstraint,
        Loop::ProcessModel& element,
        QObject* parent):
    ProcessElement{parentConstraint, parent},
    m_iscore_loop{element},
    m_deviceList{iscore::IDocument::documentFromObject(element)->model().pluginModel<DeviceDocumentPlugin>()->list()}
{

    OSSIA::TimeValue main_duration(iscore::convert::time(element.constraint().duration.defaultDuration()));

    m_ossia_loop = OSSIA::Loop::create(main_duration,
                                       [] (auto&&...) {},
    [] (auto&&...) {},
    [] (auto&&...) {}
    );

    // TODO States
    // TODO also states in BasEelement
    // TODO put graphical settings somewhere.
    m_ossia_startTimeNode = new TimeNodeElement{m_ossia_loop->getPatternStartTimeNode(), element.startTimeNode(),  m_deviceList, this};
    m_ossia_endTimeNode = new TimeNodeElement{m_ossia_loop->getPatternEndTimeNode(), element.endTimeNode(), m_deviceList, this};

    m_ossia_startEvent = new EventElement{*m_ossia_loop->getPatternStartTimeNode()->timeEvents().begin(), element.startEvent(), m_deviceList, this};
    m_ossia_endEvent = new EventElement{*m_ossia_loop->getPatternEndTimeNode()->timeEvents().begin(), element.endEvent(), m_deviceList, this};

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
