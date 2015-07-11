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
#include "Scenario/HideRackInViewModel.hpp"
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
            boost::mpl::list59<
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
            ClearState,

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

            RemoveRackFromConstraint,
            RemoveSlotFromRack,
            ClearSelection,
            RemoveSelection,
            RemoveProcessFromConstraint,
            RemoveLayerModelFromSlot,
            RemoveStateFromStateModel,

            MoveBaseEvent,
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
