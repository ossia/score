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
#include "Event/State/AddStateWithData.hpp"
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


#include <boost/mpl/list/list50.hpp>
#include <boost/mpl/aux_/config/ctps.hpp>
#include <boost/preprocessor/iterate.hpp>
#include <boost/config.hpp>

namespace boost { namespace mpl {
#define BOOST_PP_ITERATION_PARAMS_1 \
    (3,(51, 100, <boost/mpl/list/aux_/numbered.hpp>))
#include BOOST_PP_ITERATE()
}}
#include <iscore/command/CommandGeneratorMap.hpp>
#include "Control/ScenarioCommandFactory.hpp"
#include "Control/ScenarioControl.hpp"

///////////////////////////////////////////////////
//              CLASS NAME
///////////////////////////////////////////////////

// ADD CONTENT
const char* Scenario::Command::AddRackToConstraint::commandName() { return "AddRackToConstraint"; }
const char* Scenario::Command::AddSlotToRack::commandName() { return "AddSlotToRack"; }
const char* Scenario::Command::AddProcessToConstraint::commandName() { return "AddProcessToConstraint"; }
const char* Scenario::Command::AddLayerInNewSlot::commandName() { return "AddLayerInNewSlot"; }
const char* Scenario::Command::AddLayerModelToSlot::commandName() { return "AddLayerModelToSlot"; }
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
const char* Scenario::Command::MoveEvent::commandName() { return "MoveEvent"; }
//const char* Scenario::Command::MoveLayerModel::commandName() { return "MoveLayerModel"; }
const char* Scenario::Command::MoveTimeNode::commandName() { return "MoveTimeNode"; }

// REMOVE
const char* Scenario::Command::RemoveRackFromConstraint::commandName() { return "RemoveRackFromConstraint"; }
const char* Scenario::Command::RemoveSlotFromRack::commandName() { return "RemoveSlotFromRack"; }
const char* Scenario::Command::RemoveMultipleElements::commandName() { return "RemoveMultipleElements"; }
const char* Scenario::Command::RemoveProcessFromConstraint::commandName() { return "RemoveProcessFromConstraint"; }
const char* Scenario::Command::RemoveLayerModelFromSlot::commandName() { return "RemoveLayerModelFromSlot"; }

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
QString Scenario::Command::MoveEvent::description() { return QObject::tr("MoveEvent"); }
//QString Scenario::Command::MoveLayerModel::description() { return QObject::tr("MoveLayerModel"); }
QString Scenario::Command::MoveTimeNode::description() { return QObject::tr("MoveTimeNode"); }

// REMOVE
QString Scenario::Command::RemoveRackFromConstraint::description() { return QObject::tr("RemoveRackFromConstraint"); }
QString Scenario::Command::RemoveSlotFromRack::description() { return QObject::tr("RemoveSlotFromRack"); }
QString Scenario::Command::RemoveMultipleElements::description() { return QObject::tr("RemoveMultipleElements"); }
QString Scenario::Command::RemoveProcessFromConstraint::description() { return QObject::tr("RemoveProcessFromConstraint"); }
QString Scenario::Command::RemoveLayerModelFromSlot::description() { return QObject::tr("RemoveLayerModelFromSlot"); }

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


CommandGeneratorMap ScenarioCommandFactory::map;

void ScenarioControl::setupCommands()
{
    using namespace Scenario::Command;
    boost::mpl::for_each<
            boost::mpl::list61<
            AddRackToConstraint,
            AddSlotToRack,
            AddProcessToConstraint,
            AddLayerInNewSlot,
            AddLayerModelToSlot,
            AddStateToStateModel,
            AssignMessagesToState,
            AddStateWithData,

            ChangeElementColor<ConstraintModel>,
            ChangeElementColor<EventModel>,
            ChangeElementColor<TimeNodeModel>,
            ChangeElementComments<ConstraintModel>,
            ChangeElementComments<EventModel>,
            ChangeElementComments<TimeNodeModel>,
            ChangeElementLabel<ConstraintModel>,
            ChangeElementLabel<EventModel>,
            ChangeElementLabel<TimeNodeModel>,
            ChangeElementName<ConstraintModel>,
            ChangeElementName<EventModel>,
            ChangeElementName<TimeNodeModel>,
            // TODO StateModel

            ClearConstraint,
            ClearEvent,

            DuplicateRack,
            CopyConstraintContent,
            CopySlot,

            CreateState,
            CreateEvent_State,
            CreateConstraint,
            CreateConstraint_State,
            CreateConstraint_State_Event,
            CreateConstraint_State_Event_TimeNode,
            CreationMetaCommand,

            ShowRackInViewModel,
            HideRackInViewModel,

            MergeRackes,
            MergeTimeNodes,

            MoveConstraint,
            MoveSlot,
            SwapSlots,
            MoveEvent,
            MoveNewEvent,
            MoveNewState,
            MoveTimeNode,

            RemoveRackFromConstraint,
            RemoveSlotFromRack,
            RemoveMultipleElements,
            RemoveSelection,
            RemoveProcessFromConstraint,
            RemoveLayerModelFromSlot,
            RemoveStateFromStateModel,

            ResizeBaseConstraint,
            ResizeConstraint,
            ResizeSlotVertically,

            SetCondition,
            SetTrigger,

            SetMaxDuration,
            SetMinDuration,
            SetRigidity,

            SplitTimeNode,
            SwitchStatePosition,
            UnassignMessagesFromState
            >,
            boost::type<boost::mpl::_>
    >(CommandGeneratorMapInserter<ScenarioCommandFactory>());
}
