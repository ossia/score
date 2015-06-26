#include "Constraint/AddBoxToConstraint.hpp"
#include "Constraint/AddProcessToConstraint.hpp"
#include "Constraint/AddProcessViewInNewSlot.hpp"
#include "Constraint/Box/AddSlotToBox.hpp"
#include "Constraint/Box/CopySlot.hpp"
#include "Constraint/Box/Slot/AddProcessViewModelToSlot.hpp"
#include "Constraint/Box/Slot/CopyProcessViewModel.hpp"
#include "Constraint/Box/Slot/MoveProcessViewModel.hpp"
#include "Constraint/Box/Slot/RemoveProcessViewModelFromSlot.hpp"
#include "Constraint/Box/Slot/ResizeSlotVertically.hpp"
#include "Constraint/Box/MergeSlots.hpp"
#include "Constraint/Box/MoveSlot.hpp"
#include "Constraint/Box/SwapSlots.hpp"
#include "Constraint/Box/RemoveSlotFromBox.hpp"
#include "Constraint/CopyConstraintContent.hpp"
#include "Constraint/DuplicateBox.hpp"
#include "Constraint/MergeBoxes.hpp"
#include "Constraint/RemoveBoxFromConstraint.hpp"
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
#include "Scenario/Creations/CreateConstraint.hpp"
#include "Scenario/Creations/CreateEventAfterEvent.hpp"
#include "Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
#include "Scenario/Creations/CreateEventOnTimeNode.hpp"
#include "Scenario/Creations/CreationMetaCommand.hpp"
#include "Scenario/Deletions/ClearConstraint.hpp"
#include "Scenario/Deletions/ClearEvent.hpp"
#include "Scenario/Deletions/RemoveSelection.hpp"
#include "Scenario/Displacement/MoveConstraint.hpp"
#include "Scenario/Displacement/MoveEvent.hpp"
#include "Scenario/Displacement/MoveNewEvent.hpp"
#include "Scenario/Displacement/MoveTimeNode.hpp"
#include "Scenario/HideBoxInViewModel.hpp"
#include "Scenario/ResizeConstraint.hpp"
#include "Scenario/ShowBoxInViewModel.hpp"
#include "SwitchStatePosition.hpp"
#include "TimeNode/MergeTimeNodes.hpp"
#include "TimeNode/SplitTimeNode.hpp"

///////////////////////////////////////////////////
//              CLASS NAME
///////////////////////////////////////////////////

// ADD CONTENT
const char* Scenario::Command::AddBoxToConstraint::commandName() { return "AddBoxToConstraint"; }
const char* Scenario::Command::AddSlotToBox::commandName() { return "AddSlotToBox"; }
const char* Scenario::Command::AddProcessToConstraint::commandName() { return "AddProcessToConstraint"; }
const char* Scenario::Command::AddProcessViewInNewSlot::commandName() { return "AddProcessViewInNewSlot"; }
const char* Scenario::Command::AddProcessViewModelToSlot::commandName() { return "AddProcessViewModelToSlot"; }
const char* Scenario::Command::AddStateToEvent::commandName() { return "AddStateToEvent"; }
const char* Scenario::Command::AssignMessagesToState::commandName() { return "AssignMessagesToState"; }

// CLEAR CONTENT
const char* Scenario::Command::ClearConstraint::commandName() { return "ClearConstraint"; }
const char* Scenario::Command::ClearEvent::commandName() { return "ClearEvent"; }

// COPY
const char* Scenario::Command::DuplicateBox::commandName() { return "DuplicateBox"; }
const char* Scenario::Command::CopyConstraintContent::commandName() { return "CopyConstraintContent"; }
const char* Scenario::Command::CopySlot::commandName() { return "CopySlot"; }
//const char* Scenario::Command::CopyProcessViewModel::commandName() { return "CopyProcessViewModel"; }

// CREATE
const char* Scenario::Command::CreateConstraint::commandName() { return "CreateConstraint"; }
const char* Scenario::Command::CreateEventAfterEvent::commandName() { return "CreateEventAfterEvent"; }
const char* Scenario::Command::CreateEventAfterEventOnTimeNode::commandName() { return "CreateEventAfterEventOnTimeNode"; }
const char* Scenario::Command::CreateEventOnTimeNode::commandName() { return "CreateEventOnTimeNode"; }
const char* Scenario::Command::CreationMetaCommand::commandName() { return "CreationMetaCommand"; }

const char* Scenario::Command::HideBoxInViewModel::commandName() { return "HideBoxInViewModel"; }

// MERGE
const char* Scenario::Command::MergeBoxes::commandName() { return "MergeBoxes"; }
//const char* Scenario::Command::MergeSlots::commandName() { return "MergeSlots"; }
const char* Scenario::Command::MergeTimeNodes::commandName() { return "MergeTimeNodes"; }

// MOVE
const char* Scenario::Command::MoveConstraint::commandName() { return "MoveConstraint"; }
const char* Scenario::Command::MoveSlot::commandName() { return "MoveSlot"; }
const char* Scenario::Command::MoveEvent::commandName() { return "MoveEvent"; }
//const char* Scenario::Command::MoveNewEvent::commandName() { return "MoveNewEvent"; }
//const char* Scenario::Command::MoveProcessViewModel::commandName() { return "MoveProcessViewModel"; }
const char* Scenario::Command::MoveTimeNode::commandName() { return "MoveTimeNode"; }

// REMOVE
const char* Scenario::Command::RemoveBoxFromConstraint::commandName() { return "RemoveBoxFromConstraint"; }
const char* Scenario::Command::RemoveSlotFromBox::commandName() { return "RemoveSlotFromBox"; }
const char* Scenario::Command::RemoveMultipleElements::commandName() { return "RemoveMultipleElements"; }
const char* Scenario::Command::RemoveProcessFromConstraint::commandName() { return "RemoveProcessFromConstraint"; }
const char* Scenario::Command::RemoveProcessViewModelFromSlot::commandName() { return "RemoveProcessViewModelFromSlot"; }
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
const char* Scenario::Command::ShowBoxInViewModel::commandName() { return "ShowBoxInViewModel"; }
const char* Scenario::Command::SplitTimeNode::commandName() { return "SplitTimeNode"; }
const char* Scenario::Command::SwitchStatePosition::commandName() { return "SwitchStatePosition"; }
const char* Scenario::Command::UnassignMessagesFromState::commandName() { return "UnassignMessagesFromState"; }

///////////////////////////////////////////////////
//              DESCRIPTION
///////////////////////////////////////////////////

// ADD
QString Scenario::Command::AddBoxToConstraint::description() { return QObject::tr("AddBoxToConstraint"); }
QString Scenario::Command::AddSlotToBox::description() { return QObject::tr("AddSlotToBox"); }
QString Scenario::Command::AddProcessToConstraint::description() { return QObject::tr("Add process"); }
QString Scenario::Command::AddProcessViewInNewSlot::description() { return QObject::tr("AddProcessViewInNewSlot"); }
QString Scenario::Command::AddProcessViewModelToSlot::description() { return QObject::tr("AddProcessViewModelToSlot"); }
QString Scenario::Command::AddStateToEvent::description() { return QObject::tr("AddNewMessageToEvent"); }
QString Scenario::Command::AssignMessagesToState::description() { return QObject::tr("AssignMessagesToState"); }

// CLEAR
QString Scenario::Command::ClearConstraint::description() { return QObject::tr("ClearConstraint"); }
QString Scenario::Command::ClearEvent::description() { return QObject::tr("ClearEvent"); }

// COPY
QString Scenario::Command::DuplicateBox::description() { return QObject::tr("Copy a box"); }
QString Scenario::Command::CopyConstraintContent::description() { return QObject::tr("Copy constraint content"); }
QString Scenario::Command::CopySlot::description() { return QObject::tr("CopySlot"); }
//QString Scenario::Command::CopyProcessViewModel::description() { return QObject::tr("CopyProcessViewModel"); }

// CREATE
QString Scenario::Command::CreateConstraint::description() { return QObject::tr("CreateConstraint"); }
QString Scenario::Command::CreateEventAfterEvent::description() { return QObject::tr("CreateEventAfterEvent"); }
QString Scenario::Command::CreateEventAfterEventOnTimeNode::description() { return QObject::tr("CreateEventAfterEventOnTimeNode"); }
QString Scenario::Command::CreateEventOnTimeNode::description() { return QObject::tr("CreateEventOnTimeNode"); }
QString Scenario::Command::CreationMetaCommand::description() { return QObject::tr("CreationMetaCommand"); }

QString Scenario::Command::HideBoxInViewModel::description() { return QObject::tr("HideBoxInViewModel"); }

// MERGE
QString Scenario::Command::MergeBoxes::description() { return QObject::tr("MergeBoxes"); }
//QString Scenario::Command::MergeSlots::description() { return QObject::tr("MergeSlots"); }
QString Scenario::Command::MergeTimeNodes::description() { return QObject::tr("MergeTimeNodes"); }

// MOVE
QString Scenario::Command::MoveConstraint::description() { return QObject::tr("MoveConstraint"); }
QString Scenario::Command::MoveSlot::description() { return QObject::tr("MoveSlot"); }
QString Scenario::Command::MoveEvent::description() { return QObject::tr("MoveEvent"); }
//QString Scenario::Command::MoveProcessViewModel::description() { return QObject::tr("MoveProcessViewModel"); }
QString Scenario::Command::MoveTimeNode::description() { return QObject::tr("MoveTimeNode"); }

// REMOVE
QString Scenario::Command::RemoveBoxFromConstraint::description() { return QObject::tr("RemoveBoxFromConstraint"); }
QString Scenario::Command::RemoveSlotFromBox::description() { return QObject::tr("RemoveSlotFromBox"); }
QString Scenario::Command::RemoveMultipleElements::description() { return QObject::tr("RemoveMultipleElements"); }
QString Scenario::Command::RemoveProcessFromConstraint::description() { return QObject::tr("RemoveProcessFromConstraint"); }
QString Scenario::Command::RemoveProcessViewModelFromSlot::description() { return QObject::tr("RemoveProcessViewModelFromSlot"); }
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
QString Scenario::Command::ShowBoxInViewModel::description() { return QObject::tr("ShowBoxInViewModel"); }
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
    else if(name == AddBoxToConstraint::commandName()) return new AddBoxToConstraint;
    else if(name == AddSlotToBox::commandName()) return new AddSlotToBox;
    else if(name == AddProcessToConstraint::commandName()) return new AddProcessToConstraint;
    else if(name == AddProcessViewInNewSlot::commandName()) return new AddProcessViewInNewSlot;
    else if(name == AddProcessViewModelToSlot::commandName()) return new AddProcessViewModelToSlot;
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
    else if(name == DuplicateBox::commandName()) return new DuplicateBox;
    else if(name == CopyConstraintContent::commandName()) return new CopyConstraintContent;
    else if(name == CopySlot::commandName()) return new CopySlot;
 //   else if(name == CopyProcessViewModel::commandName()) return new CopyProcessViewModel;

    // CREATE ELEMENT
    else if(name == CreateConstraint::commandName()) return new CreateConstraint;
    else if(name == CreateEventAfterEvent::commandName()) return new CreateEventAfterEvent;
    else if(name == CreateEventAfterEventOnTimeNode::commandName()) return new CreateEventAfterEventOnTimeNode;
    else if(name == CreateEventOnTimeNode::commandName()) return new CreateEventOnTimeNode;
    else if(name == CreationMetaCommand::commandName()) return new CreationMetaCommand;

    else if(name == HideBoxInViewModel::commandName()) return new HideBoxInViewModel;

    // MERGE
    else if(name == MergeBoxes::commandName()) return new MergeBoxes;
//    else if(name == MergeSlots::commandName()) return new MergeSlots;
    else if(name == MergeTimeNodes::commandName()) return new MergeTimeNodes;

    // MOVE
    else if(name == MoveConstraint::commandName()) return new MoveConstraint;
    else if(name == MoveSlot::commandName()) return new MoveSlot;
    else if(name == SwapSlots::commandName()) return new SwapSlots;
    else if(name == MoveEvent::commandName()) return new MoveEvent;
//    else if(name == MoveProcessViewModel::commandName()) return new MoveProcessViewModel;
    else if(name == MoveTimeNode::commandName()) return new MoveTimeNode;

    // REMOVE ELEMENT
    else if(name == RemoveBoxFromConstraint::commandName()) return new RemoveBoxFromConstraint;
    else if(name == RemoveSlotFromBox::commandName()) return new RemoveSlotFromBox;
    else if(name == RemoveMultipleElements::commandName()) return new RemoveMultipleElements;
    else if(name == RemoveSelection::commandName()) return new RemoveSelection;
    else if(name == RemoveProcessFromConstraint::commandName()) return new RemoveProcessFromConstraint;
    else if(name == RemoveProcessViewModelFromSlot::commandName()) return new RemoveProcessViewModelFromSlot;
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
    else if(name == ShowBoxInViewModel::commandName()) return new ShowBoxInViewModel;
    else if(name == SplitTimeNode::commandName()) return new SplitTimeNode;
    else if(name == SwitchStatePosition::commandName()) return new SwitchStatePosition;
    else if(name == UnassignMessagesFromState::commandName()) return new UnassignMessagesFromState;

    else return nullptr;
}
