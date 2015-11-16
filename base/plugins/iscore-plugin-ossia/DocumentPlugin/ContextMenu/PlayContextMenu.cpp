#include "PlayContextMenu.hpp"
#include "Scenario/Control/ScenarioControl.hpp"
#include "DocumentPlugin/OSSIAStateElement.hpp"
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include "DocumentPlugin/OSSIAScenarioElement.hpp"
#include <DocumentPlugin/OSSIABaseScenarioElement.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/TimeConstraint.h>
#include <Editor/TimeNode.h>
#include <core/document/DocumentModel.hpp>
#include "DocumentPlugin/OSSIADocumentPlugin.hpp"
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <iscore2OSSIA.hpp>

PlayContextMenu::PlayContextMenu(ScenarioControl *parent):
    ScenarioActions(iscore::ToplevelMenuElement::AboutMenu, parent)
{
    m_playStates = new QAction{tr("Play (States)"), this};
    connect(m_playStates, &QAction::triggered,
            [=]()
    {
        if (auto sm = parent->focusedScenarioModel())
        {
            auto s_plugin = sm->findChild<OSSIAScenarioElement*>(QString(), Qt::FindDirectChildrenOnly);

            for(const auto& state : selectedElements(sm->states))
            {
                s_plugin->states().at(state->id())->OSSIAState()->launch();
            }
        }
    });

    m_playConstraints = new QAction{tr("Play (Constraints)"), this};
    connect(m_playConstraints, &QAction::triggered,
            [=]()
    {
        if (auto sm = parent->focusedScenarioModel())
        {
            auto s_plugin = sm->findChild<OSSIAScenarioElement*>(QString(), Qt::FindDirectChildrenOnly);

            for(const auto& constraint : selectedElements(sm->constraints))
            {
                s_plugin->constraints().at(constraint->id())->play();
            }
        }
    });
    m_playEvents = new QAction{tr("Play (Events)"), this};
    connect(m_playEvents, &QAction::triggered,
            [=]()
    {
        if (auto sm = parent->focusedScenarioModel())
        {
            auto s_plugin = sm->findChild<OSSIAScenarioElement*>(QString(), Qt::FindDirectChildrenOnly);

            for(const auto& ev : selectedElements(sm->events))
            {
                s_plugin->events().at(ev->id())->OSSIAEvent()->happen();
            }
        }
    });

    m_recordAction = new QAction{tr("Record from here"), this};
    connect(m_recordAction, &QAction::triggered,
            [=] ()
    {
        const auto& recdata = m_recordAction->data().value<ScenarioRecordInitData>();
        if(!recdata.presenter)
            return;

        auto& pres = *safe_cast<const TemporalScenarioPresenter*>(recdata.presenter);
        auto proc = safe_cast<ScenarioModel*>(&pres.layerModel().processModel());

        parent->startRecording(
                    *proc,
                    ConvertToScenarioPoint(
                        pres.view().mapFromScene(recdata.point),
                        pres.zoomRatio(),
                        pres.view().boundingRect().height()));

        m_recordAction->setData({});
    });

    m_playFromHere = new QAction{tr("Play from here"), this};
    connect(m_playFromHere, &QAction::triggered,
            [=] ()
    {
        auto baseScenar = parent->currentDocument()->model().pluginModel<OSSIADocumentPlugin>()->baseScenario();
        auto t = m_playFromHere->data().value<TimeValue>();

        baseScenar->baseConstraint()->play(t);
    });
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
    auto scenPoint = ConvertToScenarioPoint(scenept, pres.zoomRatio(), pres.view().height());
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
