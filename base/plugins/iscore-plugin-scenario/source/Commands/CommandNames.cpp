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
#include "Scenario/Deletions/RemoveConstraint.hpp"
#include "Scenario/Deletions/RemoveSelection.hpp"
#include "Scenario/Deletions/RemoveEvent.hpp"
#include "Scenario/Displacement/MoveConstraint.hpp"
#include "Scenario/Displacement/MoveEvent.hpp"
#include "Scenario/Displacement/MoveTimeNode.hpp"
#include "Scenario/HideBoxInViewModel.hpp"
#include "Scenario/ResizeConstraint.hpp"
#include "Scenario/ShowBoxInViewModel.hpp"
#include "SwitchStatePosition.hpp"
#include "TimeNode/MergeTimeNodes.hpp"
#include "TimeNode/SplitTimeNode.hpp"
#include "Scenario/Displacement/MoveEventAndConstraint.hpp"

///////////////////////////////////////////////////
//              CLASS NAME
///////////////////////////////////////////////////

// ADD CONTENT
const char* Scenario::Command::AddBoxToConstraint::className() { return "AddBoxToConstraint"; }
const char* Scenario::Command::AddDeckToBox::className() { return "AddDeckToBox"; }
const char* Scenario::Command::AddProcessToConstraint::className() { return "AddProcessToConstraint"; }
const char* Scenario::Command::AddProcessViewInNewDeck::className() { return "AddProcessViewInNewDeck"; }
const char* Scenario::Command::AddProcessViewModelToDeck::className() { return "AddProcessViewModelToDeck"; }
const char* Scenario::Command::AddStateToEvent::className() { return "AddStateToEvent"; }
const char* Scenario::Command::AssignMessagesToState::className() { return "AssignMessagesToState"; }

// CLEAR CONTENT
const char* Scenario::Command::ClearConstraint::className() { return "ClearConstraint"; }
const char* Scenario::Command::ClearEvent::className() { return "ClearEvent"; }

// COPY
const char* Scenario::Command::DuplicateBox::className() { return "DuplicateBox"; }
const char* Scenario::Command::CopyConstraintContent::className() { return "CopyConstraintContent"; }
const char* Scenario::Command::CopyDeck::className() { return "CopyDeck"; }
//const char* Scenario::Command::CopyProcessViewModel::className() { return "CopyProcessViewModel"; }

// CREATE
const char* Scenario::Command::CreateConstraint::className() { return "CreateConstraint"; }
const char* Scenario::Command::CreateEventAfterEvent::className() { return "CreateEventAfterEvent"; }
const char* Scenario::Command::CreateEventAfterEventOnTimeNode::className() { return "CreateEventAfterEventOnTimeNode"; }
const char* Scenario::Command::CreateEventOnTimeNode::className() { return "CreateEventOnTimeNode"; }
const char* Scenario::Command::CreationMetaCommand::className() { return "CreationMetaCommand"; }

const char* Scenario::Command::HideBoxInViewModel::className() { return "HideBoxInViewModel"; }

// MERGE
const char* Scenario::Command::MergeBoxes::className() { return "MergeBoxes"; }
//const char* Scenario::Command::MergeDecks::className() { return "MergeDecks"; }
const char* Scenario::Command::MergeTimeNodes::className() { return "MergeTimeNodes"; }

// MOVE
const char* Scenario::Command::MoveConstraint::className() { return "MoveConstraint"; }
const char* Scenario::Command::MoveDeck::className() { return "MoveDeck"; }
const char* Scenario::Command::MoveEvent::className() { return "MoveEvent"; }
//const char* Scenario::Command::MoveProcessViewModel::className() { return "MoveProcessViewModel"; }
const char* Scenario::Command::MoveTimeNode::className() { return "MoveTimeNode"; }

// REMOVE
const char* Scenario::Command::RemoveBoxFromConstraint::className() { return "RemoveBoxFromConstraint"; }
const char* Scenario::Command::RemoveConstraint::className() { return "RemoveConstraint"; }
const char* Scenario::Command::RemoveDeckFromBox::className() { return "RemoveDeckFromBox"; }
const char* Scenario::Command::RemoveEvent::className() { return "RemoveEvent"; }
const char* Scenario::Command::RemoveMultipleElements::className() { return "RemoveMultipleElements"; }
const char* Scenario::Command::RemoveProcessFromConstraint::className() { return "RemoveProcessFromConstraint"; }
const char* Scenario::Command::RemoveProcessViewModelFromDeck::className() { return "RemoveProcessViewModelFromDeck"; }
const char* Scenario::Command::RemoveStateFromEvent::className() { return "RemoveStateFromEvent"; }

// RESIZE
const char* Scenario::Command::ResizeBaseConstraint::className() { return "ResizeBaseConstraint"; }
const char* Scenario::Command::ResizeConstraint::className() { return "ResizeConstraint"; }
const char* Scenario::Command::ResizeDeckVertically::className() { return "ResizeDeckVertically"; }

// SET VALUE
const char* Scenario::Command::SetCondition::className() { return "SetCondition"; }
const char* Scenario::Command::SetTrigger::className() { return "SetTrigger"; }
const char* Scenario::Command::SetMaxDuration::className() { return "SetMaxDuration"; }
const char* Scenario::Command::SetMinDuration::className() { return "SetMinDuration"; }
const char* Scenario::Command::SetRigidity::className() { return "SetRigidity"; }

// OTHER
const char* Scenario::Command::ShowBoxInViewModel::className() { return "ShowBoxInViewModel"; }
const char* Scenario::Command::SplitTimeNode::className() { return "SplitTimeNode"; }
const char* Scenario::Command::SwitchStatePosition::className() { return "SwitchStatePosition"; }
const char* Scenario::Command::UnassignMessagesFromState::className() { return "UnassignMessagesFromState"; }

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
QString Scenario::Command::RemoveConstraint::description() { return QObject::tr("RemoveConstraint"); }
QString Scenario::Command::RemoveDeckFromBox::description() { return QObject::tr("RemoveDeckFromBox"); }
QString Scenario::Command::RemoveEvent::description() { return QObject::tr("RemoveEvent"); }
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
QString Scenario::Command::SetMaxDuration::description() { return QObject::tr("SetMaxDuration"); }
QString Scenario::Command::SetMinDuration::description() { return QObject::tr("SetMinDuration"); }
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

    //else if(name == Paste::className()) return new Paste;
    //else if(name == Cut::className()) return new Cut;

    // ADD CONTENTS
    else if(name == AddBoxToConstraint::className()) return new AddBoxToConstraint;
    else if(name == AddDeckToBox::className()) return new AddDeckToBox;
    else if(name == AddProcessToConstraint::className()) return new AddProcessToConstraint;
    else if(name == AddProcessViewInNewDeck::className()) return new AddProcessViewInNewDeck;
    else if(name == AddProcessViewModelToDeck::className()) return new AddProcessViewModelToDeck;
    else if(name == AddStateToEvent::className()) return new AddStateToEvent;
    else if(name == AssignMessagesToState::className()) return new AssignMessagesToState;

    // METADATA
    else if(name == ChangeElementColor<ConstraintModel>::className()) return new ChangeElementColor<ConstraintModel>;
    else if(name == ChangeElementColor<EventModel>::className()) return new ChangeElementColor<EventModel>;
    else if(name == ChangeElementColor<TimeNodeModel>::className()) return new ChangeElementColor<TimeNodeModel>;
    else if(name == ChangeElementComments<ConstraintModel>::className()) return new ChangeElementComments<ConstraintModel>;
    else if(name == ChangeElementComments<EventModel>::className()) return new ChangeElementComments<EventModel>;
    else if(name == ChangeElementComments<TimeNodeModel>::className()) return new ChangeElementComments<TimeNodeModel>;
    else if(name == ChangeElementLabel<ConstraintModel>::className()) return new ChangeElementLabel<ConstraintModel>;
    else if(name == ChangeElementLabel<EventModel>::className()) return new ChangeElementLabel<EventModel>;
    else if(name == ChangeElementLabel<TimeNodeModel>::className()) return new ChangeElementLabel<TimeNodeModel>;
    else if(name == ChangeElementName<ConstraintModel>::className()) return new ChangeElementName<ConstraintModel>;
    else if(name == ChangeElementName<EventModel>::className()) return new ChangeElementName<EventModel>;
    else if(name == ChangeElementName<TimeNodeModel>::className()) return new ChangeElementName<TimeNodeModel>;

    // CLEAR CONTENTS
    else if(name == ClearConstraint::className()) return new ClearConstraint;
    else if(name == ClearEvent::className()) return new ClearEvent;

    // COPY
    else if(name == DuplicateBox::className()) return new DuplicateBox;
    else if(name == CopyConstraintContent::className()) return new CopyConstraintContent;
    else if(name == CopyDeck::className()) return new CopyDeck;
 //   else if(name == CopyProcessViewModel::className()) return new CopyProcessViewModel;

    // CREATE ELEMENT
    else if(name == CreateConstraint::className()) return new CreateConstraint;
    else if(name == CreateEventAfterEvent::className()) return new CreateEventAfterEvent;
    else if(name == CreateEventAfterEventOnTimeNode::className()) return new CreateEventAfterEventOnTimeNode;
    else if(name == CreateEventOnTimeNode::className()) return new CreateEventOnTimeNode;
    else if(name == CreationMetaCommand::className()) return new CreationMetaCommand;

    else if(name == HideBoxInViewModel::className()) return new HideBoxInViewModel;

    // MERGE
    else if(name == MergeBoxes::className()) return new MergeBoxes;
//    else if(name == MergeDecks::className()) return new MergeDecks;
    else if(name == MergeTimeNodes::className()) return new MergeTimeNodes;

    // MOVE
    else if(name == MoveConstraint::className()) return new MoveConstraint;
    else if(name == MoveDeck::className()) return new MoveDeck;
    else if(name == SwapDecks::className()) return new SwapDecks;
    else if(name == MoveEvent::className()) return new MoveEvent;
    else if(name == MoveEventAndConstraint::className()) return new MoveEventAndConstraint;
//    else if(name == MoveProcessViewModel::className()) return new MoveProcessViewModel;
    else if(name == MoveTimeNode::className()) return new MoveTimeNode;

    // REMOVE ELEMENT
    else if(name == RemoveBoxFromConstraint::className()) return new RemoveBoxFromConstraint;
    else if(name == RemoveConstraint::className()) return new RemoveConstraint;
    else if(name == RemoveDeckFromBox::className()) return new RemoveDeckFromBox;
    else if(name == RemoveEvent::className()) return new RemoveEvent;
    else if(name == RemoveMultipleElements::className()) return new RemoveMultipleElements;
    else if(name == RemoveSelection::className()) return new RemoveSelection;
    else if(name == RemoveProcessFromConstraint::className()) return new RemoveProcessFromConstraint;
    else if(name == RemoveProcessViewModelFromDeck::className()) return new RemoveProcessViewModelFromDeck;
    else if(name == RemoveStateFromEvent::className()) return new RemoveStateFromEvent;

    // RESIZE
    else if(name == ResizeBaseConstraint::className()) return new ResizeBaseConstraint;
    else if(name == ResizeConstraint::className()) return new ResizeConstraint;
    else if(name == ResizeDeckVertically::className()) return new ResizeDeckVertically;

    // SET VALUE
    else if(name == SetCondition::className()) return new SetCondition;
    else if(name == SetTrigger::className()) return new SetTrigger;

    else if(name == SetMaxDuration::className()) return new SetMaxDuration;
    else if(name == SetMinDuration::className()) return new SetMinDuration;
    else if(name == SetRigidity::className()) return new SetRigidity;

    // OTHER
    else if(name == ShowBoxInViewModel::className()) return new ShowBoxInViewModel;
    else if(name == SplitTimeNode::className()) return new SplitTimeNode;
    else if(name == SwitchStatePosition::className()) return new SwitchStatePosition;
    else if(name == UnassignMessagesFromState::className()) return new UnassignMessagesFromState;

    else return nullptr;
}
