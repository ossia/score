#include "Constraint/AddRackToConstraint.hpp"
#include "Constraint/AddProcessToConstraint.hpp"
#include "Constraint/AddLayerInNewSlot.hpp"
#include "Constraint/Rack/AddSlotToRack.hpp"
#include "Constraint/Rack/CopySlot.hpp"
#include "Constraint/Rack/Slot/AddLayerModelToSlot.hpp"
#include "Constraint/Rack/Slot/CopyLayerModel.hpp"
#include "Constraint/Rack/Slot/MoveLayerModel.hpp"
#include "Constraint/Rack/Slot/RemoveLayerModelFromSlot.hpp"
#include "Constraint/Rack/Slot/ResizeSlotVertically.hpp"
#include "Constraint/Rack/MergeSlots.hpp"
#include "Constraint/Rack/MoveSlot.hpp"
#include "Constraint/Rack/SwapSlots.hpp"
#include "Constraint/Rack/RemoveSlotFromRack.hpp"
#include "Constraint/CopyConstraintContent.hpp"
#include "Constraint/DuplicateRack.hpp"
#include "Constraint/MergeRackes.hpp"
#include "Constraint/RemoveRackFromConstraint.hpp"
#include "Constraint/RemoveProcessFromConstraint.hpp"
#include "Constraint/SetMaxDuration.hpp"
#include "Constraint/SetMinDuration.hpp"
#include "Constraint/SetRigidity.hpp"
#include "Event/AddStateToEvent.hpp"
#include "Event/RemoveStateFromEvent.hpp"
#include "Event/SetCondition.hpp"
#include "Event/SetTrigger.hpp"
#include "Event/State/AssignMessagesToState.hpp"
#include "Event/State/UnassignMessagesFromState.hpp"
#include "RemoveMultipleElements.hpp"
#include "ResizeBaseConstraint.hpp"

#include "Scenario/Creations/CreateState.hpp"
#include "Scenario/Creations/CreateEvent_State.hpp"
#include "Scenario/Creations/CreateConstraint.hpp"
#include "Scenario/Creations/CreateConstraint_State.hpp"
#include "Scenario/Creations/CreateConstraint_State_Event.hpp"
#include "Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp"

#include "Scenario/Creations/CreationMetaCommand.hpp"
#include "Scenario/Deletions/ClearConstraint.hpp"
#include "Scenario/Deletions/ClearEvent.hpp"
#include "Scenario/Deletions/RemoveSelection.hpp"
#include "Scenario/Displacement/MoveConstraint.hpp"
#include "Scenario/Displacement/MoveEvent.hpp"
#include "Scenario/Displacement/MoveNewEvent.hpp"
#include "Scenario/Displacement/MoveNewState.hpp"
#include "Scenario/Displacement/MoveTimeNode.hpp"
#include "Scenario/HideRackInViewModel.hpp"
#include "Scenario/ResizeConstraint.hpp"
#include "Scenario/ShowRackInViewModel.hpp"
#include "SwitchStatePosition.hpp"
#include "TimeNode/MergeTimeNodes.hpp"
#include "TimeNode/SplitTimeNode.hpp"
#include "Scenario/Displacement/MoveNewEvent.hpp"
#include "Scenario/Displacement/MoveNewState.hpp"

///////////////////////////////////////////////////
//              CLASS NAME
///////////////////////////////////////////////////

// ADD CONTENT
const char* Scenario::Command::AddRackToConstraint::commandName() { return "AddRackToConstraint"; }
const char* Scenario::Command::AddSlotToRack::commandName() { return "AddSlotToRack"; }
const char* Scenario::Command::AddProcessToConstraint::commandName() { return "AddProcessToConstraint"; }
const char* Scenario::Command::AddLayerInNewSlot::commandName() { return "AddLayerInNewSlot"; }
const char* Scenario::Command::AddLayerModelToSlot::commandName() { return "AddLayerModelToSlot"; }
const char* Scenario::Command::AddStateToEvent::commandName() { return "AddStateToEvent"; }
const char* Scenario::Command::AssignMessagesToState::commandName() { return "AssignMessagesToState"; }

// CLEAR CONTENT
const char* Scenario::Command::ClearConstraint::commandName() { return "ClearConstraint"; }
const char* Scenario::Command::ClearEvent::commandName() { return "ClearEvent"; }

// COPY
const char* Scenario::Command::DuplicateRack::commandName() { return "DuplicateRack"; }
const char* Scenario::Command::CopyConstraintContent::commandName() { return "CopyConstraintContent"; }
const char* Scenario::Command::CopySlot::commandName() { return "CopySlot"; }
//const char* Scenario::Command::CopyLayerModel::commandName() { return "CopyLayerModel"; }

// CREATE
const char* Scenario::Command::CreationMetaCommand::commandName() { return "CreationMetaCommand"; }

const char* Scenario::Command::HideRackInViewModel::commandName() { return "HideRackInViewModel"; }

// MERGE
const char* Scenario::Command::MergeRackes::commandName() { return "MergeRackes"; }
//const char* Scenario::Command::MergeSlots::commandName() { return "MergeSlots"; }
const char* Scenario::Command::MergeTimeNodes::commandName() { return "MergeTimeNodes"; }

// MOVE
const char* Scenario::Command::MoveConstraint::commandName() { return "MoveConstraint"; }
const char* Scenario::Command::MoveSlot::commandName() { return "MoveSlot"; }
const char* Scenario::Command::MoveEvent::commandName() { return "MoveEvent"; }
//const char* Scenario::Command::MoveLayerModel::commandName() { return "MoveLayerModel"; }
const char* Scenario::Command::MoveTimeNode::commandName() { return "MoveTimeNode"; }

// REMOVE
const char* Scenario::Command::RemoveRackFromConstraint::commandName() { return "RemoveRackFromConstraint"; }
const char* Scenario::Command::RemoveSlotFromRack::commandName() { return "RemoveSlotFromRack"; }
const char* Scenario::Command::RemoveMultipleElements::commandName() { return "RemoveMultipleElements"; }
const char* Scenario::Command::RemoveProcessFromConstraint::commandName() { return "RemoveProcessFromConstraint"; }
const char* Scenario::Command::RemoveLayerModelFromSlot::commandName() { return "RemoveLayerModelFromSlot"; }
const char* Scenario::Command::RemoveStateFromEvent::commandName() { return "RemoveStateFromEvent"; }

// RESIZE
const char* Scenario::Command::ResizeBaseConstraint::commandName() { return "ResizeBaseConstraint"; }
const char* Scenario::Command::ResizeConstraint::commandName() { return "ResizeConstraint"; }
const char* Scenario::Command::ResizeSlotVertically::commandName() { return "ResizeSlotVertically"; }

// SET VALUE
const char* Scenario::Command::SetCondition::commandName() { return "SetCondition"; }
const char* Scenario::Command::SetTrigger::commandName() { return "SetTrigger"; }
const char* Scenario::Command::SetRigidity::commandName() { return "SetRigidity"; }

// OTHER
const char* Scenario::Command::ShowRackInViewModel::commandName() { return "ShowRackInViewModel"; }
const char* Scenario::Command::SplitTimeNode::commandName() { return "SplitTimeNode"; }
const char* Scenario::Command::SwitchStatePosition::commandName() { return "SwitchStatePosition"; }
const char* Scenario::Command::UnassignMessagesFromState::commandName() { return "UnassignMessagesFromState"; }

///////////////////////////////////////////////////
//              DESCRIPTION
///////////////////////////////////////////////////

// ADD
QString Scenario::Command::AddRackToConstraint::description() { return QObject::tr("AddRackToConstraint"); }
QString Scenario::Command::AddSlotToRack::description() { return QObject::tr("AddSlotToRack"); }
QString Scenario::Command::AddProcessToConstraint::description() { return QObject::tr("Add process"); }
QString Scenario::Command::AddLayerInNewSlot::description() { return QObject::tr("AddLayerInNewSlot"); }
QString Scenario::Command::AddLayerModelToSlot::description() { return QObject::tr("AddLayerModelToSlot"); }
QString Scenario::Command::AddStateToEvent::description() { return QObject::tr("AddNewMessageToEvent"); }
QString Scenario::Command::AssignMessagesToState::description() { return QObject::tr("AssignMessagesToState"); }

// CLEAR
QString Scenario::Command::ClearConstraint::description() { return QObject::tr("ClearConstraint"); }
QString Scenario::Command::ClearEvent::description() { return QObject::tr("ClearEvent"); }

// COPY
QString Scenario::Command::DuplicateRack::description() { return QObject::tr("Copy a rack"); }
QString Scenario::Command::CopyConstraintContent::description() { return QObject::tr("Copy constraint content"); }
QString Scenario::Command::CopySlot::description() { return QObject::tr("CopySlot"); }
//QString Scenario::Command::CopyLayerModel::description() { return QObject::tr("CopyLayerModel"); }

// CREATE
QString Scenario::Command::CreationMetaCommand::description() { return QObject::tr("CreationMetaCommand"); }

QString Scenario::Command::HideRackInViewModel::description() { return QObject::tr("HideRackInViewModel"); }

// MERGE
QString Scenario::Command::MergeRackes::description() { return QObject::tr("MergeRackes"); }
//QString Scenario::Command::MergeSlots::description() { return QObject::tr("MergeSlots"); }
QString Scenario::Command::MergeTimeNodes::description() { return QObject::tr("MergeTimeNodes"); }

// MOVE
QString Scenario::Command::MoveConstraint::description() { return QObject::tr("MoveConstraint"); }
QString Scenario::Command::MoveSlot::description() { return QObject::tr("MoveSlot"); }
QString Scenario::Command::MoveEvent::description() { return QObject::tr("MoveEvent"); }
//QString Scenario::Command::MoveLayerModel::description() { return QObject::tr("MoveLayerModel"); }
QString Scenario::Command::MoveTimeNode::description() { return QObject::tr("MoveTimeNode"); }

// REMOVE
QString Scenario::Command::RemoveRackFromConstraint::description() { return QObject::tr("RemoveRackFromConstraint"); }
QString Scenario::Command::RemoveSlotFromRack::description() { return QObject::tr("RemoveSlotFromRack"); }
QString Scenario::Command::RemoveMultipleElements::description() { return QObject::tr("RemoveMultipleElements"); }
QString Scenario::Command::RemoveProcessFromConstraint::description() { return QObject::tr("RemoveProcessFromConstraint"); }
QString Scenario::Command::RemoveLayerModelFromSlot::description() { return QObject::tr("RemoveLayerModelFromSlot"); }
QString Scenario::Command::RemoveStateFromEvent::description() { return QObject::tr("RemoveStateFromEvent"); }

// RESIZE
QString Scenario::Command::ResizeBaseConstraint::description() { return QObject::tr("ResizeBaseConstraint"); }
QString Scenario::Command::ResizeConstraint::description() { return QObject::tr("ResizeConstraint"); }
QString Scenario::Command::ResizeSlotVertically::description() { return QObject::tr("ResizeSlotVertically"); }

// SET VALUE
QString Scenario::Command::SetCondition::description() { return QObject::tr("SetCondition"); }
QString Scenario::Command::SetTrigger::description() { return QObject::tr("SetTrigger"); }
QString Scenario::Command::SetRigidity::description() { return QObject::tr("SetRigidity"); }

// OTHER
QString Scenario::Command::ShowRackInViewModel::description() { return QObject::tr("ShowRackInViewModel"); }
QString Scenario::Command::SplitTimeNode::description() { return QObject::tr("SplitTimeNode"); }
QString Scenario::Command::SwitchStatePosition::description() { return QObject::tr("SwitchStatePosition"); }
QString Scenario::Command::UnassignMessagesFromState::description() { return QObject::tr("UnassignMessagesFromState"); }

#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include "Metadata/ChangeElementColor.hpp"
#include "Metadata/ChangeElementComments.hpp"
#include "Metadata/ChangeElementLabel.hpp"
#include "Metadata/ChangeElementName.hpp"

iscore::SerializableCommand* makeCommandByName(const QString& name)
{
    using namespace Scenario::Command;
    if(false);

    //else if(name == Paste::commandName()) return new Paste;
    //else if(name == Cut::commandName()) return new Cut;

    // ADD CONTENTS
    else if(name == AddRackToConstraint::commandName()) return new AddRackToConstraint;
    else if(name == AddSlotToRack::commandName()) return new AddSlotToRack;
    else if(name == AddProcessToConstraint::commandName()) return new AddProcessToConstraint;
    else if(name == AddLayerInNewSlot::commandName()) return new AddLayerInNewSlot;
    else if(name == AddLayerModelToSlot::commandName()) return new AddLayerModelToSlot;
    else if(name == AddStateToEvent::commandName()) return new AddStateToEvent;
    else if(name == AssignMessagesToState::commandName()) return new AssignMessagesToState;

    // METADATA
    else if(name == ChangeElementColor<ConstraintModel>::commandName()) return new ChangeElementColor<ConstraintModel>;
    else if(name == ChangeElementColor<EventModel>::commandName()) return new ChangeElementColor<EventModel>;
    else if(name == ChangeElementColor<TimeNodeModel>::commandName()) return new ChangeElementColor<TimeNodeModel>;
    else if(name == ChangeElementComments<ConstraintModel>::commandName()) return new ChangeElementComments<ConstraintModel>;
    else if(name == ChangeElementComments<EventModel>::commandName()) return new ChangeElementComments<EventModel>;
    else if(name == ChangeElementComments<TimeNodeModel>::commandName()) return new ChangeElementComments<TimeNodeModel>;
    else if(name == ChangeElementLabel<ConstraintModel>::commandName()) return new ChangeElementLabel<ConstraintModel>;
    else if(name == ChangeElementLabel<EventModel>::commandName()) return new ChangeElementLabel<EventModel>;
    else if(name == ChangeElementLabel<TimeNodeModel>::commandName()) return new ChangeElementLabel<TimeNodeModel>;
    else if(name == ChangeElementName<ConstraintModel>::commandName()) return new ChangeElementName<ConstraintModel>;
    else if(name == ChangeElementName<EventModel>::commandName()) return new ChangeElementName<EventModel>;
    else if(name == ChangeElementName<TimeNodeModel>::commandName()) return new ChangeElementName<TimeNodeModel>;

    // CLEAR CONTENTS
    else if(name == ClearConstraint::commandName()) return new ClearConstraint;
    else if(name == ClearEvent::commandName()) return new ClearEvent;

    // COPY
    else if(name == DuplicateRack::commandName()) return new DuplicateRack;
    else if(name == CopyConstraintContent::commandName()) return new CopyConstraintContent;
    else if(name == CopySlot::commandName()) return new CopySlot;
 //   else if(name == CopyLayerModel::commandName()) return new CopyLayerModel;

    // CREATE ELEMENT
    else if(name == CreateState::commandName()) return new CreateState;
    else if(name == CreateEvent_State::commandName()) return new CreateEvent_State;
    else if(name == CreateConstraint::commandName()) return new CreateConstraint;
    else if(name == CreateConstraint_State::commandName()) return new CreateConstraint_State;
    else if(name == CreateConstraint_State_Event::commandName()) return new CreateConstraint_State_Event;
    else if(name == CreateConstraint_State_Event_TimeNode::commandName()) return new CreateConstraint_State_Event_TimeNode;
    else if(name == CreationMetaCommand::commandName()) return new CreationMetaCommand;

    else if(name == HideRackInViewModel::commandName()) return new HideRackInViewModel;

    // MERGE
    else if(name == MergeRackes::commandName()) return new MergeRackes;
//    else if(name == MergeSlots::commandName()) return new MergeSlots;
    else if(name == MergeTimeNodes::commandName()) return new MergeTimeNodes;

    // MOVE
    else if(name == MoveConstraint::commandName()) return new MoveConstraint;
    else if(name == MoveSlot::commandName()) return new MoveSlot;
    else if(name == SwapSlots::commandName()) return new SwapSlots;
    else if(name == MoveEvent::commandName()) return new MoveEvent;
    else if(name == MoveNewEvent::commandName()) return new MoveNewEvent;
    else if(name == MoveNewState::commandName()) return new MoveNewState;
//    else if(name == MoveLayerModel::commandName()) return new MoveLayerModel;
    else if(name == MoveTimeNode::commandName()) return new MoveTimeNode;

    // REMOVE ELEMENT
    else if(name == RemoveRackFromConstraint::commandName()) return new RemoveRackFromConstraint;
    else if(name == RemoveSlotFromRack::commandName()) return new RemoveSlotFromRack;
    else if(name == RemoveMultipleElements::commandName()) return new RemoveMultipleElements;
    else if(name == RemoveSelection::commandName()) return new RemoveSelection;
    else if(name == RemoveProcessFromConstraint::commandName()) return new RemoveProcessFromConstraint;
    else if(name == RemoveLayerModelFromSlot::commandName()) return new RemoveLayerModelFromSlot;
    else if(name == RemoveStateFromEvent::commandName()) return new RemoveStateFromEvent;

    // RESIZE
    else if(name == ResizeBaseConstraint::commandName()) return new ResizeBaseConstraint;
    else if(name == ResizeConstraint::commandName()) return new ResizeConstraint;
    else if(name == ResizeSlotVertically::commandName()) return new ResizeSlotVertically;

    // SET VALUE
    else if(name == SetCondition::commandName()) return new SetCondition;
    else if(name == SetTrigger::commandName()) return new SetTrigger;

    else if(name == SetMaxDuration::commandName()) return new SetMaxDuration;
    else if(name == SetMinDuration::commandName()) return new SetMinDuration;
    else if(name == SetRigidity::commandName()) return new SetRigidity;

    // OTHER
    else if(name == ShowRackInViewModel::commandName()) return new ShowRackInViewModel;
    else if(name == SplitTimeNode::commandName()) return new SplitTimeNode;
    else if(name == SwitchStatePosition::commandName()) return new SwitchStatePosition;
    else if(name == UnassignMessagesFromState::commandName()) return new UnassignMessagesFromState;

    else return nullptr;
}
