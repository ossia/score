#include "Paste.hpp"
const char* Scenario::Command::Paste::className() { return "Paste"; }

#include "Cut.hpp"
const char* Scenario::Command::Cut::className() { return "Cut"; }

#include "Constraint/AddProcessToConstraint.hpp"
const char* Scenario::Command::AddProcessToConstraint::className() { return "AddProcessToConstraint"; }

#include "Constraint/CopyBox.hpp"
const char* Scenario::Command::CopyBox::className() { return "CopyBox"; }

#include "Constraint/RemoveProcessFromConstraint.hpp"
const char* Scenario::Command::RemoveProcessFromConstraint::className() { return "RemoveProcessFromConstraint"; }

#include "Constraint/Box/AddDeckToBox.hpp"
const char* Scenario::Command::AddDeckToBox::className() { return "AddDeckToBox"; }

#include "Constraint/Box/Deck/RemoveProcessViewModelFromDeck.hpp"
const char* Scenario::Command::RemoveProcessViewModelFromDeck::className() { return "RemoveProcessViewModelFromDeck"; }

#include "Constraint/Box/Deck/MoveProcessViewModel.hpp"
const char* Scenario::Command::MoveProcessViewModel::className() { return "MoveProcessViewModel"; }

#include "Constraint/Box/Deck/ResizeDeckVertically.hpp"
const char* Scenario::Command::ResizeDeckVertically::className() { return "ResizeDeckVertically"; }

#include "Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"
const char* Scenario::Command::AddProcessViewModelToDeck::className() { return "AddProcessViewModelToDeck"; }

#include "Constraint/Box/Deck/CopyProcessViewModel.hpp"
const char* Scenario::Command::CopyProcessViewModel::className() { return "CopyProcessViewModel"; }

#include "Constraint/Box/MoveDeck.hpp"
const char* Scenario::Command::MoveDeck::className() { return "MoveDeck"; }

#include "Constraint/Box/MergeDecks.hpp"
const char* Scenario::Command::MergeDecks::className() { return "MergeDecks"; }

#include "Constraint/Box/CopyDeck.hpp"
const char* Scenario::Command::CopyDeck::className() { return "CopyDeck"; }

#include "Constraint/Box/RemoveDeckFromBox.hpp"
const char* Scenario::Command::RemoveDeckFromBox::className() { return "RemoveDeckFromBox"; }

#include "Constraint/RemoveBoxFromConstraint.hpp"
const char* Scenario::Command::RemoveBoxFromConstraint::className() { return "RemoveBoxFromConstraint"; }

#include "Constraint/AddProcessViewInNewDeck.hpp"
const char* Scenario::Command::AddProcessViewInNewDeck::className() { return "AddProcessViewInNewDeck"; }

#include "Constraint/SetRigidity.hpp"
const char* Scenario::Command::SetRigidity::className() { return "SetRigidity"; }

#include "Constraint/AddBoxToConstraint.hpp"
const char* Scenario::Command::AddBoxToConstraint::className() { return "AddBoxToConstraint"; }

#include "Constraint/SetMaxDuration.hpp"
const char* Scenario::Command::SetMaxDuration::className() { return "SetMaxDuration"; }

#include "Constraint/SetMinDuration.hpp"
const char* Scenario::Command::SetMinDuration::className() { return "SetMinDuration"; }

#include "Constraint/MergeBoxes.hpp"
const char* Scenario::Command::MergeBoxes::className() { return "MergeBoxes"; }

#include "Event/RemoveStateFromEvent.hpp"
const char* Scenario::Command::RemoveStateFromEvent::className() { return "RemoveStateFromEvent"; }

#include "Event/SetCondition.hpp"
const char* Scenario::Command::SetCondition::className() { return "SetCondition"; }

#include "Event/AddStateToEvent.hpp"
const char* Scenario::Command::AddStateToEvent::className() { return "AddStateToEvent"; }

#include "Event/State/UnassignMessagesFromState.hpp"
const char* Scenario::Command::UnassignMessagesFromState::className() { return "UnassignMessagesFromState"; }

#include "Event/State/AssignMessagesToState.hpp"
const char* Scenario::Command::AssignMessagesToState::className() { return "AssignMessagesToState"; }

#include "ResizeBaseConstraint.hpp"
const char* Scenario::Command::ResizeBaseConstraint::className() { return "ResizeBaseConstraint"; }

#include "RemoveMultipleElements.hpp"
const char* Scenario::Command::RemoveMultipleElements::className() { return "RemoveMultipleElements"; }

#include "TimeNode/MergeTimeNodes.hpp"
const char* Scenario::Command::MergeTimeNodes::className() { return "MergeTimeNodes"; }

#include "TimeNode/SplitTimeNode.hpp"
const char* Scenario::Command::SplitTimeNode::className() { return "SplitTimeNode"; }

#include "SwitchStatePosition.hpp"
const char* Scenario::Command::SwitchStatePosition::className() { return "SwitchStatePosition"; }

#include "Scenario/Deletions/RemoveEvent.hpp"
const char* Scenario::Command::RemoveEvent::className() { return "RemoveEvent"; }

#include "Scenario/Deletions/RemoveConstraint.hpp"
const char* Scenario::Command::RemoveConstraint::className() { return "RemoveConstraint"; }

#include "Scenario/Creations/CreateEventAfterEvent.hpp"
const char* Scenario::Command::CreateEventAfterEvent::className() { return "CreateEventAfterEvent"; }

#include "Scenario/Displacement/MoveEvent.hpp"
const char* Scenario::Command::MoveEvent::className() { return "MoveEvent"; }

#include "Scenario/Displacement/MoveTimeNode.hpp"
const char* Scenario::Command::MoveTimeNode::className() { return "MoveTimeNode"; }

#include "Scenario/ResizeConstraint.hpp"
const char* Scenario::Command::ResizeConstraint::className() { return "ResizeConstraint"; }

#include "Scenario/Displacement/MoveConstraint.hpp"
const char* Scenario::Command::MoveConstraint::className() { return "MoveConstraint"; }

#include "Scenario/Creations/CreateConstraint.hpp"
const char* Scenario::Command::CreateConstraint::className() { return "CreateConstraint"; }

#include "Scenario/Deletions/ClearEvent.hpp"
const char* Scenario::Command::ClearEvent::className() { return "ClearEvent"; }

#include "Scenario/Creations/CreateEvent.hpp"
const char* Scenario::Command::CreateEvent::className() { return "CreateEvent"; }

#include "Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
const char* Scenario::Command::CreateEventAfterEventOnTimeNode::className() { return "CreateEventAfterEventOnTimeNode"; }

#include "Scenario/Deletions/ClearConstraint.hpp"
const char* Scenario::Command::ClearConstraint::className() { return "ClearConstraint"; }

#include "Scenario/ShowBoxInViewModel.hpp"
const char* Scenario::Command::ShowBoxInViewModel::className() { return "ShowBoxInViewModel"; }

#include "Scenario/HideBoxInViewModel.hpp"
const char* Scenario::Command::HideBoxInViewModel::className() { return "HideBoxInViewModel"; }

QString Scenario::Command::Paste::description() { return "Paste"; }
QString Scenario::Command::Cut::description() { return "Cut"; }
QString Scenario::Command::AddProcessToConstraint::description() { return QObject::tr("Add process"); }
QString Scenario::Command::CopyBox::description() { return QObject::tr("Copy a box"); }
QString Scenario::Command::RemoveProcessFromConstraint::description() { return QObject::tr("RemoveProcessFromConstraint"); }
QString Scenario::Command::AddDeckToBox::description() { return QObject::tr("AddDeckToBox"); }
QString Scenario::Command::RemoveProcessViewModelFromDeck::description() { return QObject::tr("RemoveProcessViewModelFromDeck"); }
QString Scenario::Command::MoveProcessViewModel::description() { return QObject::tr("MoveProcessViewModel"); }
QString Scenario::Command::ResizeDeckVertically::description() { return QObject::tr("ResizeDeckVertically"); }
QString Scenario::Command::AddProcessViewModelToDeck::description() { return QObject::tr("AddProcessViewModelToDeck"); }
QString Scenario::Command::CopyProcessViewModel::description() { return QObject::tr("CopyProcessViewModel"); }
QString Scenario::Command::MoveDeck::description() { return QObject::tr("MoveDeck"); }
QString Scenario::Command::MergeDecks::description() { return QObject::tr("MergeDecks"); }
QString Scenario::Command::CopyDeck::description() { return QObject::tr("CopyDeck"); }
QString Scenario::Command::RemoveDeckFromBox::description() { return QObject::tr("RemoveDeckFromBox"); }
QString Scenario::Command::RemoveBoxFromConstraint::description() { return QObject::tr("RemoveBoxFromConstraint"); }
QString Scenario::Command::AddProcessViewInNewDeck::description() { return QObject::tr("AddProcessViewInNewDeck"); }
QString Scenario::Command::SetRigidity::description() { return QObject::tr("SetRigidity"); }
QString Scenario::Command::AddBoxToConstraint::description() { return QObject::tr("AddBoxToConstraint"); }
QString Scenario::Command::SetMaxDuration::description() { return QObject::tr("SetMaxDuration"); }
QString Scenario::Command::SetMinDuration::description() { return QObject::tr("SetMinDuration"); }
QString Scenario::Command::MergeBoxes::description() { return QObject::tr("MergeBoxes"); }
QString Scenario::Command::RemoveStateFromEvent::description() { return QObject::tr("RemoveStateFromEvent"); }
QString Scenario::Command::SetCondition::description() { return QObject::tr("SetCondition"); }
QString Scenario::Command::AddStateToEvent::description() { return QObject::tr("AddNewMessageToEvent"); }
QString Scenario::Command::UnassignMessagesFromState::description() { return QObject::tr("UnassignMessagesFromState"); }
QString Scenario::Command::AssignMessagesToState::description() { return QObject::tr("AssignMessagesToState"); }
QString Scenario::Command::ResizeBaseConstraint::description() { return QObject::tr("ResizeBaseConstraint"); }
QString Scenario::Command::RemoveMultipleElements::description() { return QObject::tr("RemoveMultipleElements"); }
QString Scenario::Command::MergeTimeNodes::description() { return QObject::tr("MergeTimeNodes"); }
QString Scenario::Command::SplitTimeNode::description() { return QObject::tr("SplitTimeNode"); }
QString Scenario::Command::SwitchStatePosition::description() { return QObject::tr("SwitchStatePosition"); }

QString Scenario::Command::RemoveEvent::description() { return QObject::tr("RemoveEvent"); }
QString Scenario::Command::RemoveConstraint::description() { return QObject::tr("RemoveConstraint"); }
QString Scenario::Command::CreateEventAfterEvent::description() { return QObject::tr("CreateEventAfterEvent"); }
QString Scenario::Command::MoveEvent::description() { return QObject::tr("MoveEvent"); }
QString Scenario::Command::MoveTimeNode::description() { return QObject::tr("MoveTimeNode"); }
QString Scenario::Command::ResizeConstraint::description() { return QObject::tr("ResizeConstraint"); }
QString Scenario::Command::MoveConstraint::description() { return QObject::tr("MoveConstraint"); }
QString Scenario::Command::CreateConstraint::description() { return QObject::tr("CreateConstraint"); }
QString Scenario::Command::ClearEvent::description() { return QObject::tr("ClearEvent"); }
QString Scenario::Command::CreateEvent::description() { return QObject::tr("CreateEvent"); }
QString Scenario::Command::CreateEventAfterEventOnTimeNode::description() { return QObject::tr("CreateEventAfterEventOnTimeNode"); }
QString Scenario::Command::ClearConstraint::description() { return QObject::tr("ClearConstraint"); }
QString Scenario::Command::ShowBoxInViewModel::description() { return QObject::tr("ShowBoxInViewModel"); }
QString Scenario::Command::HideBoxInViewModel::description() { return QObject::tr("HideBoxInViewModel"); }

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
    else if(name == AddProcessToConstraint::className()) return new AddProcessToConstraint;
    else if(name == CopyBox::className()) return new CopyBox;
    else if(name == RemoveProcessFromConstraint::className()) return new RemoveProcessFromConstraint;
    else if(name == AddDeckToBox::className()) return new AddDeckToBox;
    else if(name == RemoveProcessViewModelFromDeck::className()) return new RemoveProcessViewModelFromDeck;
    else if(name == MoveProcessViewModel::className()) return new MoveProcessViewModel;
    else if(name == ResizeDeckVertically::className()) return new ResizeDeckVertically;
    else if(name == AddProcessViewModelToDeck::className()) return new AddProcessViewModelToDeck;
    else if(name == CopyProcessViewModel::className()) return new CopyProcessViewModel;
    else if(name == MoveDeck::className()) return new MoveDeck;
    else if(name == MergeDecks::className()) return new MergeDecks;
    else if(name == CopyDeck::className()) return new CopyDeck;
    else if(name == RemoveDeckFromBox::className()) return new RemoveDeckFromBox;
    else if(name == RemoveBoxFromConstraint::className()) return new RemoveBoxFromConstraint;
    else if(name == AddProcessViewInNewDeck::className()) return new AddProcessViewInNewDeck;
    else if(name == SetRigidity::className()) return new SetRigidity;
    else if(name == AddBoxToConstraint::className()) return new AddBoxToConstraint;
    else if(name == SetMaxDuration::className()) return new SetMaxDuration;
    else if(name == SetMinDuration::className()) return new SetMinDuration;
    else if(name == MergeBoxes::className()) return new MergeBoxes;
    else if(name == RemoveStateFromEvent::className()) return new RemoveStateFromEvent;
    else if(name == SetCondition::className()) return new SetCondition;
    else if(name == AddStateToEvent::className()) return new AddStateToEvent;
    else if(name == UnassignMessagesFromState::className()) return new UnassignMessagesFromState;
    else if(name == AssignMessagesToState::className()) return new AssignMessagesToState;
    else if(name == ResizeBaseConstraint::className()) return new ResizeBaseConstraint;
    else if(name == RemoveMultipleElements::className()) return new RemoveMultipleElements;
    else if(name == MergeTimeNodes::className()) return new MergeTimeNodes;
    else if(name == SplitTimeNode::className()) return new SplitTimeNode;
    else if(name == SwitchStatePosition::className()) return new SwitchStatePosition;

    else if(name == ChangeElementComments<ConstraintModel>::className()) return new ChangeElementComments<ConstraintModel>;
    else if(name == ChangeElementLabel<ConstraintModel>::className()) return new ChangeElementLabel<ConstraintModel>;
    else if(name == ChangeElementColor<ConstraintModel>::className()) return new ChangeElementColor<ConstraintModel>;
    else if(name == ChangeElementName<ConstraintModel>::className()) return new ChangeElementName<ConstraintModel>;

    else if(name == ChangeElementComments<EventModel>::className()) return new ChangeElementComments<EventModel>;
    else if(name == ChangeElementLabel<EventModel>::className()) return new ChangeElementLabel<EventModel>;
    else if(name == ChangeElementColor<EventModel>::className()) return new ChangeElementColor<EventModel>;
    else if(name == ChangeElementName<EventModel>::className()) return new ChangeElementName<EventModel>;

    else if(name == ChangeElementComments<TimeNodeModel>::className()) return new ChangeElementComments<TimeNodeModel>;
    else if(name == ChangeElementLabel<TimeNodeModel>::className()) return new ChangeElementLabel<TimeNodeModel>;
    else if(name == ChangeElementColor<TimeNodeModel>::className()) return new ChangeElementColor<TimeNodeModel>;
    else if(name == ChangeElementName<TimeNodeModel>::className()) return new ChangeElementName<TimeNodeModel>;

    else if(name == RemoveEvent::className()) return new RemoveEvent;
    else if(name == RemoveConstraint::className()) return new RemoveConstraint;
    else if(name == CreateEventAfterEvent::className()) return new CreateEventAfterEvent;
    else if(name == MoveEvent::className()) return new MoveEvent;
    else if(name == MoveTimeNode::className()) return new MoveTimeNode;
    else if(name == ResizeConstraint::className()) return new ResizeConstraint;
    else if(name == MoveConstraint::className()) return new MoveConstraint;
    else if(name == CreateConstraint::className()) return new CreateConstraint;
    else if(name == ClearEvent::className()) return new ClearEvent;
    else if(name == CreateEvent::className()) return new CreateEvent;
    else if(name == CreateEventAfterEventOnTimeNode::className()) return new CreateEventAfterEventOnTimeNode;
    else if(name == ClearConstraint::className()) return new ClearConstraint;
    else if(name == ShowBoxInViewModel::className()) return new ShowBoxInViewModel;
    else if(name == HideBoxInViewModel::className()) return new HideBoxInViewModel;

    else return nullptr;
}
