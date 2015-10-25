#include "ObjectMenuActions.hpp"

#include <core/document/DocumentModel.hpp>

#include "iscore/menu/MenuInterface.hpp"

#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/Trigger/TriggerModel.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"

#include "Commands/Constraint/ReplaceConstraintContent.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/TimeNode/AddTrigger.hpp"
#include "Commands/TimeNode/RemoveTrigger.hpp"

#include "Control/ScenarioControl.hpp"
#include <Commands/Cohesion/InterpolateStates.hpp>
#include <Commands/Cohesion/UpdateStates.hpp>

#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>

#include <QTextEdit>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QTextBlock>
#include <QDialog>
#include "TextDialog.hpp"

ObjectMenuActions::ObjectMenuActions(
        iscore::ToplevelMenuElement menuElt,
        ScenarioControl* parent) :
    ScenarioActions(menuElt, parent)
{
    // REMOVE
    m_removeElements = new QAction{tr("Remove selected elements"), this};
    m_removeElements->setShortcut(Qt::Key_Backspace);
    m_removeElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_removeElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
            mgr.removeSelection(*sm);
        }
    });

    m_clearElements = new QAction{tr("Clear selected elements"), this};
    m_clearElements->setShortcut(QKeySequence::Delete);
    m_clearElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_clearElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
            mgr.clearContentFromSelection(*sm);
        }
    });

    // COPY/CUT
    m_copyContent = new QAction{tr("Copy"), this};
    m_copyContent->setShortcut(QKeySequence::Copy);
    m_copyContent->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_copyContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{copySelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    m_cutContent = new QAction{tr("Cut"), this};
    m_cutContent->setShortcut(QKeySequence::Cut);
    m_cutContent->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_cutContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{cutSelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    m_pasteContent = new QAction{tr("Paste content"), this};
    //m_pasteContent->setShortcut(QKeySequence::Paste);
    m_pasteContent->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_pasteContent, &QAction::triggered,
            [this]()
    {
        writeJsonToSelectedElements(
                    QJsonDocument::fromJson(
                        QApplication::clipboard()->text().toUtf8()).object());
    });

    // DISPLAY JSON
    m_elementsToJson = new QAction{tr("Convert selection to JSON"), this};
    connect(m_elementsToJson, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{copySelectedElementsToJson()};
        auto s = new TextDialog{doc.toJson(QJsonDocument::Indented), qApp->activeWindow()};

        s->show();
    });

    // ADD PROCESS
    m_addProcessDialog = new AddProcessDialog(qApp->activeWindow());

    connect(m_addProcessDialog, &AddProcessDialog::okPressed,
            this, &ObjectMenuActions::addProcessInConstraint);

    m_addProcess = new QAction{tr("Add Process in constraint"), this};
    connect(m_addProcess, &QAction::triggered,
            [this]()
    {
        auto selectedConstraints = selectedElements(m_parent->focusedScenarioModel()->constraints);
        if(selectedConstraints.isEmpty())
            return;
        m_addProcessDialog->launchWindow();
    });


    m_interp = new QAction {tr("Interpolate states"), this};
    m_interp->setShortcutContext(Qt::ApplicationShortcut);
    m_interp->setShortcut(tr("Ctrl+K"));
    m_interp->setToolTip(tr("Ctrl+K"));
    connect(m_interp, &QAction::triggered,
            this, [&] () {
        InterpolateStates(m_parent->currentDocument());
    });


    m_updateStates = new QAction {tr("Refresh states"), this};
    m_updateStates->setShortcutContext(Qt::ApplicationShortcut);
    m_updateStates->setShortcut(tr("Ctrl+U"));
    m_updateStates->setToolTip(tr("Ctrl+U"));
    connect(m_updateStates, &QAction::triggered,
            this, [&] () {
        RefreshStates(m_parent->currentDocument());
    });


    // ADD TRIGGER
    m_addTrigger = new QAction{tr("Add Trigger"), this};
    connect(m_addTrigger, &QAction::triggered,
            this, &ObjectMenuActions::addTriggerToTimeNode);

    m_removeTrigger = new QAction{tr("Remove Trigger"), this};
    connect(m_removeTrigger, &QAction::triggered,
            this, &ObjectMenuActions::removeTriggerFromTimeNode);

}

void ObjectMenuActions::fillMenuBar(iscore::MenubarManager* menu)
{
    menu->insertActionIntoToplevelMenu(m_menuElt, m_addProcess);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_addTrigger);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_removeTrigger);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_elementsToJson);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_removeElements);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_clearElements);
    menu->addSeparatorIntoToplevelMenu(m_menuElt, iscore::EditMenuElement::Separator_Copy);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_copyContent);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_cutContent);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_pasteContent);

    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_interp);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_updateStates);
}

#include <Commands/Scenario/ShowRackInViewModel.hpp>
#include <Commands/Scenario/HideRackInViewModel.hpp>
#include <Process/Temporal/TemporalScenarioLayerModel.hpp>
void ObjectMenuActions::fillContextMenu(
        QMenu *menu,
        const Selection& sel,
        const TemporalScenarioPresenter& pres,
        const QPoint&,
        const QPointF& scenePoint)
{
    if(!sel.empty())
    {
        QList<const ConstraintModel*> selectedConstraints = filterSelectionByType<ConstraintModel>(sel);
        if(selectedConstraints.size() == 1)
        {
            auto rackMenu = menu->addMenu(tr("Rack"));
            auto& cst = *selectedConstraints.front();

            // We have to find the constraint view model of this layer.
            auto& vm = dynamic_cast<const TemporalScenarioLayerModel*>(&pres.layerModel())->constraint(cst.id());

            for(const RackModel& rack : cst.racks)
            {
                auto act = new QAction{rack.objectName(), rackMenu};
                connect(act, &QAction::triggered,
                        this, [&] () {
                    auto cmd = new Scenario::Command::ShowRackInViewModel{vm, rack.id()};
                    CommandDispatcher<> dispatcher{m_parent->currentDocument()->commandStack()};
                    dispatcher.submitCommand(cmd);
                });

                rackMenu->addAction(act);
            }

            auto hideAct = new QAction{tr("Hide"), rackMenu};
            connect(hideAct, &QAction::triggered,
                    this, [&] () {
                auto cmd = new Scenario::Command::HideRackInViewModel{vm};
                CommandDispatcher<> dispatcher{m_parent->currentDocument()->commandStack()};
                dispatcher.submitCommand(cmd);
            });
            rackMenu->addAction(hideAct);
        }

        if(selectedConstraints.size() >= 1)
        {
            menu->addAction(m_addProcess);
            menu->addAction(m_interp);
            menu->addSeparator();
        }


        if(std::any_of(sel.cbegin(),
                       sel.cend(),
                       [] (const QObject* obj) { return dynamic_cast<const EventModel*>(obj); })) // TODO : event or timenode ?
        {
            menu->addAction(m_addTrigger);
            menu->addAction(m_removeTrigger);
            menu->addSeparator();
        }

        if(std::any_of(sel.cbegin(),
                       sel.cend(),
                       [] (const QObject* obj) { return dynamic_cast<const StateModel*>(obj); })) // TODO : event or timenode ?
        {
            menu->addAction(m_updateStates);
            menu->addSeparator();
        }

        menu->addAction(m_elementsToJson);
        menu->addAction(m_removeElements);
        menu->addAction(m_clearElements);
        menu->addSeparator();

        menu->addAction(m_copyContent);
        menu->addAction(m_cutContent);
        menu->addAction(m_pasteContent);
    }

    auto pasteElements = new QAction{tr("Paste here"), this};
    pasteElements->setShortcut(QKeySequence::Paste);
    pasteElements->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteElements, &QAction::triggered,
            [&,scenePoint]()
    {
        this->pasteElements(QJsonDocument::fromJson(QApplication::clipboard()->text().toUtf8()).object(),
                      ConvertToScenarioPoint(scenePoint, pres.zoomRatio(), pres.view().boundingRect().height()));
    });
    menu->addAction(pasteElements);

}

bool ObjectMenuActions::populateToolBar(QToolBar * b)
{
    b->addAction(m_interp);
    return true;
}

void ObjectMenuActions::setEnabled(bool b)
{
    for (auto& act : actions())
    {
        act->setEnabled(b);
    }
}

template<typename Selected_T>
auto arrayToJson(Selected_T &&selected)
{
    QJsonArray array;
    if (!selected.empty())
    {
        for (const auto &element : selected)
        {
            Visitor<Reader<JSONObject>> jr;
            jr.readFrom(*element);
            array.push_back(jr.m_obj);
        }
    }

    return array;
}

QJsonObject ObjectMenuActions::copySelectedElementsToJson()
{
    QJsonObject base;

    if (auto sm = m_parent->focusedScenarioModel())
    {
        auto selectedConstraints = selectedElements(sm->constraints);
        auto selectedEvents = selectedElements(sm->events);
        auto selectedTimeNodes = selectedElements(sm->timeNodes);
        auto selectedStates = selectedElements(sm->states);

        for(const ConstraintModel* constraint : selectedConstraints)
        {
            auto start_it = std::find_if(selectedStates.begin(), selectedStates.end(), [&] (const StateModel* state) { return state->id() == constraint->startState();});
            if(start_it == selectedStates.end())
            {
                selectedStates.push_back(&sm->states.at(constraint->startState()));
            }

            auto end_it = std::find_if(selectedStates.begin(), selectedStates.end(), [&] (const StateModel* state) { return state->id() == constraint->endState();});
            if(end_it == selectedStates.end())
            {
                selectedStates.push_back(&sm->states.at(constraint->endState()));
            }
        }

        for(const StateModel* state : selectedStates)
        {
            auto ev_it = std::find_if(selectedEvents.begin(), selectedEvents.end(), [&] (const EventModel* event) { return state->eventId() == event->id(); });
            if(ev_it == selectedEvents.end())
            {
                selectedEvents.push_back(&sm->events.at(state->eventId()));
            }

            // If the previous or next constraint is not here, we set it to null in a copy.
        }
        for(const EventModel* event : selectedEvents)
        {
            auto tn_it = std::find_if(selectedTimeNodes.begin(), selectedTimeNodes.end(), [&] (const TimeNodeModel* tn) { return tn->id() == event->timeNode(); });
            if(tn_it == selectedTimeNodes.end())
            {
                selectedTimeNodes.push_back(&sm->timeNodes.at(event->timeNode()));
            }

            // If some events aren't there, we set them to null in a copy.
        }

        std::vector<TimeNodeModel*> copiedTimeNodes;
        copiedTimeNodes.reserve(selectedTimeNodes.size());
        for(const auto& tn : selectedTimeNodes)
        {
            auto clone_tn = new TimeNodeModel(*tn, tn->id(), sm->parent());
            auto events = clone_tn->events();
            for(const auto& event : events)
            {
                auto absent = std::none_of(selectedEvents.begin(), selectedEvents.end(), [&] (const EventModel* ev) { return ev->id() == event; });
                if(absent)
                    clone_tn->removeEvent(event);
            }

            copiedTimeNodes.push_back(clone_tn);
        }


        std::vector<EventModel*> copiedEvents;
        copiedEvents.reserve(selectedEvents.size());
        for(const auto& ev : selectedEvents)
        {
            auto clone_ev = new EventModel(*ev, ev->id(), sm->parent());
            auto states = clone_ev->states();
            for(const auto& state : states)
            {
                auto absent = std::none_of(selectedStates.begin(), selectedStates.end(), [&] (const StateModel* st) { return st->id() == state; });
                if(absent)
                    clone_ev->removeState(state);
            }

            copiedEvents.push_back(clone_ev);
        }

        std::vector<StateModel*> copiedStates;
        copiedStates.reserve(selectedStates.size());
        for(const auto& st : selectedStates)
        {
            auto clone_st = new StateModel(*st, st->id(), sm->parent());
            auto prev_absent = std::none_of(selectedConstraints.begin(), selectedConstraints.end(), [&] (const ConstraintModel* cst) { return cst->id() == st->previousConstraint(); });
            if(prev_absent)
                clone_st->setPreviousConstraint(Id<ConstraintModel>{});
            auto next_absent = std::none_of(selectedConstraints.begin(), selectedConstraints.end(), [&] (const ConstraintModel* cst) { return cst->id() == st->nextConstraint(); });
            if(next_absent)
                clone_st->setNextConstraint(Id<ConstraintModel>{});

            copiedStates.push_back(clone_st);
        }


        base["Constraints"] = arrayToJson(selectedConstraints);
        base["Events"] = arrayToJson(copiedEvents);
        base["TimeNodes"] = arrayToJson(copiedTimeNodes);
        base["States"] = arrayToJson(copiedStates);

        for(auto elt : copiedTimeNodes)
            delete elt;
        for(auto elt : copiedEvents)
            delete elt;
        for(auto elt : copiedStates)
            delete elt;
    }
    else
    {
        // Full-view copy
        auto& bem = iscore::IDocument::modelDelegate<BaseElementModel>(*m_parent->currentDocument());
        if(bem.baseConstraint().selection.get())
        {
            QJsonArray arr;
            Visitor<Reader<JSONObject>> jr;
            jr.readFrom(bem.baseConstraint());
            arr.push_back(jr.m_obj);
            base["Constraints"] = arr;
        }
    }
    return base;
}

QJsonObject ObjectMenuActions::cutSelectedElementsToJson()
{
    auto obj = copySelectedElementsToJson();

    if (auto sm = m_parent->focusedScenarioModel())
    {
        ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
        mgr.clearContentFromSelection(*sm);
    }

    return obj;
}


// MOVEME
// TODO add me to command lists
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
// Needed for copy since we want to generate IDs that are neither
// in the scenario in which we are copying into, nor in the elements
// that we copied because it may cause conflicts.
template<typename T, typename Vector1, typename Vector2>
auto getStrongIdRange2(std::size_t s, const Vector1& existing1, const Vector2& existing2)
{
    std::vector<Id<T>> vec;
    vec.reserve(s + existing1.size() + existing2.size());
    std::transform(existing1.begin(), existing1.end(), std::back_inserter(vec),
                   [] (const auto& elt) { return elt.id(); });
    std::transform(existing2.begin(), existing2.end(), std::back_inserter(vec),
                   [] (const auto& elt) { return elt->id(); });

    for(; s --> 0 ;)
    {
        vec.push_back(getStrongId(vec));
    }

    return std::vector<Id<T>>(vec.begin() + existing1.size() + existing2.size(), vec.end());
}

class ScenarioPasteElements : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                         ScenarioPasteElements,
                                         "ScenarioPasteElements")

    public:
        ScenarioPasteElements(
                Path<TemporalScenarioLayerModel>&& path,
                const QJsonObject& obj,
                const ScenarioPoint& pt):
            iscore::SerializableCommand{factoryName(), commandName(), description()},
            m_ts{std::move(path)}
        {

            // We assign new ids WRT the elements of the scenario - these ids can
            // be easily mapped.
            const auto& tsModel = m_ts.find();
            const ScenarioModel& scenario = ::model(tsModel);

            // TODO the elements are child of the document
            // because else the State cannot be constructed properly
            // (it calls iscore::IDocument::commandStack...). This is ugly.
            auto doc = iscore::IDocument::documentFromObject(scenario);

            // We deserialize everything
            {
                auto json_arr = obj["Constraints"].toArray();
                m_constraints.reserve(json_arr.size());
                for(const auto& element : json_arr)
                {
                    m_constraints.emplace_back(new ConstraintModel{Deserializer<JSONObject>{element.toObject()}, doc});
                }
            }
            {
                auto json_arr = obj["TimeNodes"].toArray();
                m_timenodes.reserve(json_arr.size());
                for(const auto& element : json_arr)
                {
                    m_timenodes.emplace_back(new TimeNodeModel{Deserializer<JSONObject>{element.toObject()}, doc});
                }
            }
            {
                auto json_arr = obj["Events"].toArray();
                m_events.reserve(json_arr.size());
                for(const auto& element : json_arr)
                {
                    m_events.emplace_back(new EventModel{Deserializer<JSONObject>{element.toObject()}, doc});
                }
            }
            {
                auto json_arr = obj["States"].toArray();
                m_states.reserve(json_arr.size());
                for(const auto& element : json_arr)
                {
                    m_states.emplace_back(new StateModel{Deserializer<JSONObject>{element.toObject()}, doc});
                }
            }



            auto constraint_ids = getStrongIdRange2<ConstraintModel>(m_constraints.size(), scenario.constraints, m_constraints);
            auto timenode_ids = getStrongIdRange2<TimeNodeModel>(m_timenodes.size(), scenario.timeNodes, m_timenodes);
            auto event_ids = getStrongIdRange2<EventModel>(m_events.size(), scenario.events, m_events);
            auto state_ids = getStrongIdRange2<StateModel>(m_states.size(), scenario.states, m_states);

            {
                int i = 0;
                for(TimeNodeModel* timenode : m_timenodes)
                {
                    for(EventModel* event : m_events)
                    {
                        if(event->timeNode() == timenode->id())
                        {
                            event->changeTimeNode(timenode_ids[i]);
                        }
                    }

                    timenode->setId(timenode_ids[i]);
                    i++;
                }
            }

            {
                int i = 0;
                for(EventModel* event : m_events)
                {
                    {
                        auto it = std::find_if(m_timenodes.begin(),
                                               m_timenodes.end(),
                                               [&] (TimeNodeModel* tn) { return tn->id() == event->timeNode(); });
                        ISCORE_ASSERT(it != m_timenodes.end());
                        auto timenode = *it;
                        timenode->removeEvent(event->id());
                        timenode->addEvent(event_ids[i]);
                    }

                    for(StateModel* state : m_states)
                    {
                        if(state->eventId() == event->id())
                        {
                            state->setEventId(event_ids[i]);
                        }
                    }

                    event->setId(event_ids[i]);
                    i++;
                }
            }

            {
                int i = 0;
                for(StateModel* state : m_states)
                {
                    {
                        auto it = std::find_if(m_events.begin(),
                                               m_events.end(),
                                               [&] (EventModel* event) { return event->id() == state->eventId(); });
                        ISCORE_ASSERT(it != m_events.end());
                        auto event = *it;
                        event->removeState(state->id());
                        event->addState(state_ids[i]);
                    }

                    for(ConstraintModel* constraint : m_constraints)
                    {
                        if(constraint->startState() == state->id())
                            constraint->setStartState(state_ids[i]);
                        else if(constraint->endState() == state->id())
                            constraint->setEndState(state_ids[i]);
                    }

                    state->setId(state_ids[i]);
                    i++;
                }
            }

            {
                int i = 0;
                for(ConstraintModel* constraint : m_constraints)
                {
                    for(StateModel* state : m_states)
                    {
                        if(state->id() == constraint->startState())
                        {
                            state->setNextConstraint(constraint_ids[i]);
                        }
                        else if(state->id() == constraint->endState())
                        {
                            state->setPreviousConstraint(constraint_ids[i]);
                        }
                    }

                    constraint->setId(constraint_ids[i]);
                    i++;
                }
            }


            // Then we have to create default constraint views... everywhere...
            for(ConstraintModel* constraint : m_constraints)
            {
                auto res = m_constraintViewModels.insert(std::make_pair(constraint->id(), ConstraintViewModelIdMap{}));
                ISCORE_ASSERT(res.second);

                for(const auto& viewModel : layers(scenario))
                {
                    res.first->second[*viewModel] = getStrongId(viewModel->constraints());
                }
            }
            // Set the correct positions / dates.
            // Take the earliest constraint and compute the delta; apply the delta everywhere.

            // Same for y.

            // TODO if a constraint does not have its beginning / end state
            // selected, it should have a new one created.
        }

        void undo() const override
        {

        }

        void redo() const override
        {
            auto& tsModel = m_ts.find();
            ScenarioModel& scenario = ::model(tsModel);
            for(const auto& timenode : m_timenodes)
            {
                scenario.timeNodes.add(new TimeNodeModel(*timenode, timenode->id(), &scenario));
            }
            for(const auto& event : m_events)
            {
                scenario.events.add(new EventModel(*event, event->id(), &scenario));
            }
            for(const auto& state : m_states)
            {
                scenario.states.add(new StateModel(*state, state->id(), &scenario));
            }
            for(const auto& constraint : m_constraints)
            {
                scenario.constraints.add(new ConstraintModel(*constraint, constraint->id(), &scenario));

                createConstraintViewModels(m_constraintViewModels.at(constraint->id()),
                                           constraint->id(),
                                           scenario);
            }
        }

    protected:
        void serializeImpl(QDataStream&) const override
        {
        }
        void deserializeImpl(QDataStream&) override
        {
        }

    private:
        Path<TemporalScenarioLayerModel> m_ts;
        std::vector<TimeNodeModel*> m_timenodes;
        std::vector<ConstraintModel*> m_constraints;
        std::vector<EventModel*> m_events;
        std::vector<StateModel*> m_states;

        std::map<Id<ConstraintModel>, ConstraintViewModelIdMap> m_constraintViewModels;
};


void ObjectMenuActions::pasteElements(
        const QJsonObject& obj,
        const ScenarioPoint& origin)
{
    // TODO check for unnecessary uses of focusedProcessModel after focusedPresenter.
    auto pres = m_parent->focusedPresenter();
    if(!pres)
        return;

    auto& sm = static_cast<const TemporalScenarioLayerModel&>(pres->layerModel());
    // TODO check json validity
    auto cmd = new ScenarioPasteElements(sm, obj, origin);

    dispatcher().submitCommand(cmd);
}

#include <Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
// MOVEME
// TODO add me to command lists
class ScenarioPasteContent : public iscore::AggregateCommand
{
        ISCORE_AGGREGATE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                      ScenarioPasteContent,
                                      "ScenarioPasteContent")
};

// MOVEME
class InsertContentInState : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), InsertContentInState, "InsertContentInState")

    public:
       InsertContentInState(
                const QJsonObject& stateData,
                Path<StateModel>&& targetState):
          iscore::SerializableCommand{factoryName(), commandName(), description()},
          m_state{std::move(targetState)}
        {
          // TODO ask what should be copied ? the state due to the processes ? the user state ?
          // For now we copy the whole value.
          // First recreate the tree

          // TODO we should update the processes here, and provide an API to do this
          // properly.

          auto& state = m_state.find();

          m_oldNode = state.messages().rootNode();
          m_newNode = m_oldNode;
          updateTreeWithMessageList(
                      m_newNode,
                      flatten(unmarshall<MessageNode>(stateData["Messages"].toObject()))
                  );
        }

        void undo() const override
        {
            auto& state = m_state.find();
            state.messages() = m_oldNode;
        }

        void redo() const override
        {
            auto& state = m_state.find();
            state.messages() = m_newNode;
        }

    protected:
        void serializeImpl(QDataStream& s) const override
        {
            s << m_oldNode << m_newNode << m_state;
        }

        void deserializeImpl(QDataStream& s) override
        {
            s >> m_oldNode >> m_newNode >> m_state;
        }

        private:
        MessageNode m_oldNode;
        MessageNode m_newNode;
        Path<StateModel> m_state;
};

void ObjectMenuActions::writeJsonToSelectedElements(const QJsonObject &obj)
{
    auto pres = m_parent->focusedPresenter();
    if(!pres)
        return;

    auto sm = m_parent->focusedScenarioModel();

    MacroCommandDispatcher dispatcher{new ScenarioPasteContent, this->dispatcher().stack()};
    auto selectedConstraints = selectedElements(sm->constraints);
    for(const auto& json_vref : obj["Constraints"].toArray())
    {
        for(const auto& constraint : selectedConstraints)
        {
            auto cmd = new Scenario::Command::InsertContentInConstraint{
                       json_vref.toObject(),
                       *constraint,
                       pres->stateMachine().expandMode()};

            dispatcher.submitCommand(cmd);
        }
    }

    auto selectedStates = selectedElements(sm->states);
    for(const auto& json_vref : obj["States"].toArray())
    {
        for(const auto& state : selectedStates)
        {
            auto cmd = new InsertContentInState{
                       json_vref.toObject(),
                       *state};

            dispatcher.submitCommand(cmd);
        }
    }

    dispatcher.commit();
}

void ObjectMenuActions::addProcessInConstraint(QString processName)
{
    auto selectedConstraints = selectedElements(m_parent->focusedScenarioModel()->constraints);
    if(selectedConstraints.isEmpty())
        return;
    auto cmd = new Scenario::Command::AddProcessToConstraint //NOTE just the first, not all ?
    {
        **selectedConstraints.begin(),
        processName
    };
    emit dispatcher().submitCommand(cmd);
}

void ObjectMenuActions::addTriggerToTimeNode()
{
    auto selectedTimeNodes = selectedElements(m_parent->focusedScenarioModel()->timeNodes);// TODO : event or timenode ?
    if(selectedTimeNodes.isEmpty())
        return;

    auto cmd = new Scenario::Command::AddTrigger{**selectedTimeNodes.begin()};
    emit dispatcher().submitCommand(cmd);
}

void ObjectMenuActions::removeTriggerFromTimeNode()
{
    auto selectedTimeNodes = selectedElements(m_parent->focusedScenarioModel()->timeNodes);// TODO : event or timenode ?
    if(selectedTimeNodes.isEmpty())
        return;

    auto cmd = new Scenario::Command::RemoveTrigger{**selectedTimeNodes.begin()};
    emit dispatcher().submitCommand(cmd);
}

CommandDispatcher<> ObjectMenuActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->commandStack()};
    return disp;
}


QList<QAction*> ObjectMenuActions::actions() const
{
    return {
            m_removeElements,
            m_clearElements,
            m_copyContent,
            m_cutContent,
            m_pasteContent,
            m_elementsToJson,
            m_addProcess,
            m_addTrigger
        };
}

