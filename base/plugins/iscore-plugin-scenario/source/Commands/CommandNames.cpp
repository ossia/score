#include "Constraint/AddBoxToConstraint.hpp"
#include "Constraint/AddProcessToConstraint.hpp"
#include "Constraint/AddProcessViewInNewDeck.hpp"
#include "Constraint/Box/AddDeckToBox.hpp"
#include "Constraint/Box/CopyDeck.hpp"
#include "Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"
#include "Constraint/Box/Deck/CopyProcessViewModel.hpp"
#include "Constraint/Box/Deck/MoveProcessViewModel.hpp"
#include "Constraint/Box/Deck/RemoveProcessViewModelFromDeck.hpp"
#include "Constraint/Box/Deck/ResizeDeckVertically.hpp"
#include "Constraint/Box/MergeDecks.hpp"
#include "Constraint/Box/MoveDeck.hpp"
#include "Constraint/Box/SwapDecks.hpp"
#include "Constraint/Box/RemoveDeckFromBox.hpp"
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
const char* Scenario::Command::AddDeckToBox::commandName() { return "AddDeckToBox"; }
const char* Scenario::Command::AddProcessToConstraint::commandName() { return "AddProcessToConstraint"; }
const char* Scenario::Command::AddProcessViewInNewDeck::commandName() { return "AddProcessViewInNewDeck"; }
const char* Scenario::Command::AddProcessViewModelToDeck::commandName() { return "AddProcessViewModelToDeck"; }
const char* Scenario::Command::AddStateToEvent::commandName() { return "AddStateToEvent"; }
const char* Scenario::Command::AssignMessagesToState::commandName() { return "AssignMessagesToState"; }

// CLEAR CONTENT
const char* Scenario::Command::ClearConstraint::commandName() { return "ClearConstraint"; }
const char* Scenario::Command::ClearEvent::commandName() { return "ClearEvent"; }

// COPY
const char* Scenario::Command::DuplicateBox::commandName() { return "DuplicateBox"; }
const char* Scenario::Command::CopyConstraintContent::commandName() { return "CopyConstraintContent"; }
const char* Scenario::Command::CopyDeck::commandName() { return "CopyDeck"; }
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
//const char* Scenario::Command::MergeDecks::commandName() { return "MergeDecks"; }
const char* Scenario::Command::MergeTimeNodes::commandName() { return "MergeTimeNodes"; }

// MOVE
const char* Scenario::Command::MoveConstraint::commandName() { return "MoveConstraint"; }
const char* Scenario::Command::MoveDeck::commandName() { return "MoveDeck"; }
const char* Scenario::Command::MoveEvent::commandName() { return "MoveEvent"; }
//const char* Scenario::Command::MoveNewEvent::commandName() { return "MoveNewEvent"; }
//const char* Scenario::Command::MoveProcessViewModel::commandName() { return "MoveProcessViewModel"; }
const char* Scenario::Command::MoveTimeNode::commandName() { return "MoveTimeNode"; }

// REMOVE
const char* Scenario::Command::RemoveBoxFromConstraint::commandName() { return "RemoveBoxFromConstraint"; }
const char* Scenario::Command::RemoveDeckFromBox::commandName() { return "RemoveDeckFromBox"; }
const char* Scenario::Command::RemoveMultipleElements::commandName() { return "RemoveMultipleElements"; }
const char* Scenario::Command::RemoveProcessFromConstraint::commandName() { return "RemoveProcessFromConstraint"; }
const char* Scenario::Command::RemoveProcessViewModelFromDeck::commandName() { return "RemoveProcessViewModelFromDeck"; }
const char* Scenario::Command::RemoveStateFromEvent::commandName() { return "RemoveStateFromEvent"; }

// RESIZE
const char* Scenario::Command::ResizeBaseConstraint::commandName() { return "ResizeBaseConstraint"; }
const char* Scenario::Command::ResizeConstraint::commandName() { return "ResizeConstraint"; }
const char* Scenario::Command::ResizeDeckVertically::commandName() { return "ResizeDeckVertically"; }

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
QString Scenario::Command::AddDeckToBox::description() { return QObject::tr("AddDeckToBox"); }
QString Scenario::Command::AddProcessToConstraint::description() { return QObject::tr("Add process"); }
QString Scenario::Command::AddProcessViewInNewDeck::description() { return QObject::tr("AddProcessViewInNewDeck"); }
QString Scenario::Command::AddProcessViewModelToDeck::description() { return QObject::tr("AddProcessViewModelToDeck"); }
QString Scenario::Command::AddStateToEvent::description() { return QObject::tr("AddNewMessageToEvent"); }
QString Scenario::Command::AssignMessagesToState::description() { return QObject::tr("AssignMessagesToState"); }

// CLEAR
QString Scenario::Command::ClearConstraint::description() { return QObject::tr("ClearConstraint"); }
QString Scenario::Command::ClearEvent::description() { return QObject::tr("ClearEvent"); }

// COPY
QString Scenario::Command::DuplicateBox::description() { return QObject::tr("Copy a box"); }
QString Scenario::Command::CopyConstraintContent::description() { return QObject::tr("Copy constraint content"); }
QString Scenario::Command::CopyDeck::description() { return QObject::tr("CopyDeck"); }
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
//QString Scenario::Command::MergeDecks::description() { return QObject::tr("MergeDecks"); }
QString Scenario::Command::MergeTimeNodes::description() { return QObject::tr("MergeTimeNodes"); }

// MOVE
QString Scenario::Command::MoveConstraint::description() { return QObject::tr("MoveConstraint"); }
QString Scenario::Command::MoveDeck::description() { return QObject::tr("MoveDeck"); }
QString Scenario::Command::MoveEvent::description() { return QObject::tr("MoveEvent"); }
//QString Scenario::Command::MoveProcessViewModel::description() { return QObject::tr("MoveProcessViewModel"); }
QString Scenario::Command::MoveTimeNode::description() { return QObject::tr("MoveTimeNode"); }

// REMOVE
QString Scenario::Command::RemoveBoxFromConstraint::description() { return QObject::tr("RemoveBoxFromConstraint"); }
QString Scenario::Command::RemoveDeckFromBox::description() { return QObject::tr("RemoveDeckFromBox"); }
QString Scenario::Command::RemoveMultipleElements::description() { return QObject::tr("RemoveMultipleElements"); }
QString Scenario::Command::RemoveProcessFromConstraint::description() { return QObject::tr("RemoveProcessFromConstraint"); }
QString Scenario::Command::RemoveProcessViewModelFromDeck::description() { return QObject::tr("RemoveProcessViewModelFromDeck"); }
QString Scenario::Command::RemoveStateFromEvent::description() { return QObject::tr("RemoveStateFromEvent"); }

// RESIZE
QString Scenario::Command::ResizeBaseConstraint::description() { return QObject::tr("ResizeBaseConstraint"); }
QString Scenario::Command::ResizeConstraint::description() { return QObject::tr("ResizeConstraint"); }
QString Scenario::Command::ResizeDeckVertically::description() { return QObject::tr("ResizeDeckVertically"); }

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
    else if(name == AddDeckToBox::commandName()) return new AddDeckToBox;
    else if(name == AddProcessToConstraint::commandName()) return new AddProcessToConstraint;
    else if(name == AddProcessViewInNewDeck::commandName()) return new AddProcessViewInNewDeck;
    else if(name == AddProcessViewModelToDeck::commandName()) return new AddProcessViewModelToDeck;
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
    else if(name == CopyDeck::commandName()) return new CopyDeck;
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
//    else if(name == MergeDecks::commandName()) return new MergeDecks;
    else if(name == MergeTimeNodes::commandName()) return new MergeTimeNodes;

    // MOVE
    else if(name == MoveConstraint::commandName()) return new MoveConstraint;
    else if(name == MoveDeck::commandName()) return new MoveDeck;
    else if(name == SwapDecks::commandName()) return new SwapDecks;
    else if(name == MoveEvent::commandName()) return new MoveEvent;
//    else if(name == MoveProcessViewModel::commandName()) return new MoveProcessViewModel;
    else if(name == MoveTimeNode::commandName()) return new MoveTimeNode;

    // REMOVE ELEMENT
    else if(name == RemoveBoxFromConstraint::commandName()) return new RemoveBoxFromConstraint;
    else if(name == RemoveDeckFromBox::commandName()) return new RemoveDeckFromBox;
    else if(name == RemoveMultipleElements::commandName()) return new RemoveMultipleElements;
    else if(name == RemoveSelection::commandName()) return new RemoveSelection;
    else if(name == RemoveProcessFromConstraint::commandName()) return new RemoveProcessFromConstraint;
    else if(name == RemoveProcessViewModelFromDeck::commandName()) return new RemoveProcessViewModelFromDeck;
    else if(name == RemoveStateFromEvent::commandName()) return new RemoveStateFromEvent;

    // RESIZE
    else if(name == ResizeBaseConstraint::commandName()) return new ResizeBaseConstraint;
    else if(name == ResizeConstraint::commandName()) return new ResizeConstraint;
    else if(name == ResizeDeckVertically::commandName()) return new ResizeDeckVertically;

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
