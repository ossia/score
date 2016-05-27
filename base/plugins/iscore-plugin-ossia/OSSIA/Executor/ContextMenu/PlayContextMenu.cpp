#include <Editor/State.h>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <QAction>
#include <QList>
#include <QMenu>
#include <qnamespace.h>

#include <QRect>
#include <QString>
#include <QVariant>
#include <algorithm>
#include <map>
#include <memory>

#include <OSSIA/Executor/StateElement.hpp>
#include <OSSIA/Executor/ConstraintElement.hpp>
#include <OSSIA/Executor/EventElement.hpp>
#include <OSSIA/Executor/ScenarioElement.hpp>
#include <OSSIA/Executor/StateElement.hpp>

#include "Editor/TimeEvent.h"
#include "PlayContextMenu.hpp"
#include <Process/LayerModel.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioRecordInitData.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>

#include <iscore/menu/MenuInterface.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioRecordInitData.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <OSSIA/OSSIAApplicationPlugin.hpp>

namespace RecreateOnPlay
{
PlayContextMenu::PlayContextMenu(
        OSSIAApplicationPlugin& plug,
        const iscore::ApplicationContext& ctx):
  m_ctx{ctx}
{
    auto& exec_signals = m_ctx.components.applicationPlugin<Scenario::ScenarioApplicationPlugin>().execution();

    using namespace Scenario;
    m_playStates = new QAction{tr("Play (States)"), this};
    connect(m_playStates, &QAction::triggered,
            [=]()
    {
        const auto& ctx = m_ctx.documents.currentDocument()->context();
        auto sm = focusedScenarioInterface(ctx);
        if (sm)
        {
            auto& r_ctx = ctx.plugin<RecreateOnPlay::DocumentPlugin>().context();

            for(const StateModel* state : selectedElements(sm->getStates()))
            {
                auto ossia_state = iscore::convert::state(*state, r_ctx);
                ossia_state->launch();
            }
        }
    });

    con(exec_signals, &Scenario::ScenarioExecution::playState,
        this, [=] (const Scenario::ScenarioInterface& scenar, const Id<StateModel>& stateId)
    {
        const auto& ctx = m_ctx.documents.currentDocument()->context();
        auto& r_ctx = ctx.plugin<RecreateOnPlay::DocumentPlugin>().context();

        auto ossia_state = iscore::convert::state(
                    scenar.state(stateId),
                    r_ctx);
        ossia_state->launch();
    });

    m_playConstraints = new QAction{tr("Play (Constraints)"), this};
    connect(m_playConstraints, &QAction::triggered,
            [=]()
    {
        /*
        if (auto sm = parent->focusedScenarioModel())
        {
            auto s_plugin = sm->findChild<OSSIAScenarioElement*>(QString(), Qt::FindDirectChildrenOnly);

            for(const auto& constraint : selectedElements(sm->constraints))
            {
                s_plugin->constraints().at(constraint->id())->play();
            }
        }
        */
    });
    m_playEvents = new QAction{tr("Play (Events)"), this};
    connect(m_playEvents, &QAction::triggered,
            [=]()
    {
        /*
        if (auto sm = parent->focusedScenarioModel())
        {
            auto s_plugin = sm->findChild<OSSIAScenarioElement*>(QString(), Qt::FindDirectChildrenOnly);

            for(const auto& ev : selectedElements(sm->events))
            {
                s_plugin->events().at(ev->id())->OSSIAEvent()->happen();
            }
        }
        */
    });

    m_recordAutomations = new QAction{tr("Record automations from here"), this};
    connect(m_recordAutomations, &QAction::triggered,
            [=,&exec_signals] ()
    {
        const auto& recdata = m_recordAutomations->data().value<ScenarioRecordInitData>();
        if(!recdata.presenter)
            return;

        auto& pres = *safe_cast<const TemporalScenarioPresenter*>(recdata.presenter);
        auto proc = safe_cast<Scenario::ScenarioModel*>(&pres.layerModel().processModel());

        exec_signals.startRecording(
                    *proc,
                    Scenario::ConvertToScenarioPoint(
                        pres.view().mapFromScene(recdata.point),
                        pres.zoomRatio(),
                        pres.view().boundingRect().height()));

        m_recordAutomations->setData({});
    });

    m_recordMessages = new QAction{tr("Record messages from here"), this};
    connect(m_recordMessages, &QAction::triggered,
            [=,&exec_signals] ()
    {
        const auto& recdata = m_recordMessages->data().value<ScenarioRecordInitData>();
        if(!recdata.presenter)
            return;

        auto& pres = *safe_cast<const TemporalScenarioPresenter*>(recdata.presenter);
        auto proc = safe_cast<Scenario::ScenarioModel*>(&pres.layerModel().processModel());

        exec_signals.startRecordingMessages(
                    *proc,
                    Scenario::ConvertToScenarioPoint(
                        pres.view().mapFromScene(recdata.point),
                        pres.zoomRatio(),
                        pres.view().boundingRect().height()));

        m_recordAutomations->setData({});
    });

    m_playFromHere = new QAction{tr("Play from here"), this};
    connect(m_playFromHere, &QAction::triggered,
        this, [&] ()
    {
        auto t = m_playFromHere->data().value<::TimeValue>();
        plug.on_play(true, t);
    });
}

void PlayContextMenu::fillContextMenu(
        QMenu *menu,
        const Selection & s,
        const Scenario::TemporalScenarioPresenter& pres,
        const QPoint&,
        const QPointF& scenept)
{
    using namespace Scenario;
    menu->addAction(m_playFromHere);
    auto scenPoint = Scenario::ConvertToScenarioPoint(scenept, pres.zoomRatio(), pres.view().height());
    m_playFromHere->setData(QVariant::fromValue(scenPoint.date));

    if(s.empty())
    {
        menu->addAction(m_recordAutomations);
        menu->addAction(m_recordMessages);

        auto data = QVariant::fromValue(ScenarioRecordInitData{&pres, scenept});
        m_recordAutomations->setData(data);
        m_recordMessages->setData(data);
    }
}

// TODO use me

void PlayContextMenu::setupContextMenu(Process::LayerContextMenuManager &ctxm)
{
    using namespace Process;
    Process::LayerContextMenu& scenario_cm = ctxm.menu<ContextMenus::ScenarioModelContextMenu>();
    Process::LayerContextMenu& state_cm = ctxm.menu<ContextMenus::StateContextMenu>();

    state_cm.functions.push_back(
    [this] (QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx)
    {
        using namespace iscore;
        auto sel = ctx.context.selectionStack.currentSelection();
        if(sel.empty())
            return;

        if(any_of(sel, matches<Scenario::StateModel>{}))
        {
            auto stateSubmenu = menu.findChild<QMenu*>("State");
            ISCORE_ASSERT(stateSubmenu);

            stateSubmenu->addAction(m_playStates);
        }
    });

    scenario_cm.functions.push_back(
    [this] (QMenu& menu, QPoint, QPointF scenept, const Process::LayerContext& ctx)
    {
        auto& pres = safe_cast<Scenario::TemporalScenarioPresenter&>(ctx.layerPresenter);
        auto scenPoint = Scenario::ConvertToScenarioPoint(scenept, pres.zoomRatio(), pres.view().height());
        m_playFromHere->setData(QVariant::fromValue(scenPoint.date));
        menu.addAction(m_playFromHere);

        auto sel = ctx.context.selectionStack.currentSelection();
        if(!sel.empty())
            return;

        menu.addAction(m_recordAutomations);
        menu.addAction(m_recordMessages);

        auto data = QVariant::fromValue(Scenario::ScenarioRecordInitData{&pres, scenept});
        m_recordAutomations->setData(data);
        m_recordMessages->setData(data);

    });


}

}
