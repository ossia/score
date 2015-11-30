#include <API/Headers/Editor/State.h>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <boost/optional/optional.hpp>
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

#include <OSSIA/RecreateOnPlayDocumentPlugin/StateElement.hpp>
#include <OSSIA/RecreateOnPlayDocumentPlugin/ConstraintElement.hpp>
#include <OSSIA/RecreateOnPlayDocumentPlugin/EventElement.hpp>
#include <OSSIA/RecreateOnPlayDocumentPlugin/ScenarioElement.hpp>
#include <OSSIA/RecreateOnPlayDocumentPlugin/StateElement.hpp>

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
#include <iscore/tools/NotifyingMap.hpp>
#include <OSSIA/iscore2OSSIA.hpp>

namespace iscore {
class MenubarManager;
}  // namespace iscore

PlayContextMenu::PlayContextMenu(ScenarioApplicationPlugin *parent):
    ScenarioActions(iscore::ToplevelMenuElement::AboutMenu, parent)
{
    m_playStates = new QAction{tr("Play (States)"), this};
    connect(m_playStates, &QAction::triggered,
            [=]()
    {
        if (auto sm = parent->focusedScenarioModel())
        {
            const auto& ctx = iscore::IDocument::documentContext(*sm);

            for(const StateModel* state : selectedElements(sm->states))
            {
                auto ossia_state = iscore::convert::state(
                            state->messages().rootNode(),
                            ctx.document.model().pluginModel<DeviceDocumentPlugin>()->list());
                ossia_state->launch();
            }
        }
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

    m_recordAction = new QAction{tr("Record from here"), this};
    connect(m_recordAction, &QAction::triggered,
            [=] ()
    {
        const auto& recdata = m_recordAction->data().value<ScenarioRecordInitData>();
        if(!recdata.presenter)
            return;

        auto& pres = *safe_cast<const TemporalScenarioPresenter*>(recdata.presenter);
        auto proc = safe_cast<Scenario::ScenarioModel*>(&pres.layerModel().processModel());

        parent->startRecording(
                    *proc,
                    Scenario::ConvertToScenarioPoint(
                        pres.view().mapFromScene(recdata.point),
                        pres.zoomRatio(),
                        pres.view().boundingRect().height()));

        m_recordAction->setData({});
    });

    m_playFromHere = new QAction{tr("Play from here"), this};
}

void PlayContextMenu::fillMenuBar(iscore::MenubarManager *menu)
{

}

void PlayContextMenu::fillContextMenu(
        QMenu *menu,
        const Selection & s,
        const TemporalScenarioPresenter& pres,
        const QPoint& pt,
        const QPointF& scenept)
{
    menu->addAction(m_playFromHere);
    auto scenPoint = Scenario::ConvertToScenarioPoint(scenept, pres.zoomRatio(), pres.view().height());
    m_playFromHere->setData(QVariant::fromValue(scenPoint.date));

    if(s.empty())
    {
        menu->addAction(m_recordAction);
        m_recordAction->setData(QVariant::fromValue(ScenarioRecordInitData{&pres, scenept}));
    }
    else
    {
        if(std::any_of(s.cbegin(), s.cend(), [] (auto obj) { return dynamic_cast<const StateModel*>(obj.data());}))
        {
            menu->addAction(m_playStates);
        }
        /*
    if(std::any_of(s.cbegin(), s.cend(), [] (auto obj) { return dynamic_cast<const ConstraintModel*>(obj);}))
    {
        menu->addAction(m_playConstraints);
    }
    if(std::any_of(s.cbegin(), s.cend(), [] (auto obj) { return dynamic_cast<const EventModel*>(obj);}))
    {
        menu->addAction(m_playEvents);
    }
    */
    }
}


void PlayContextMenu::setEnabled(bool b)
{
    m_playStates->setEnabled(b);
}
