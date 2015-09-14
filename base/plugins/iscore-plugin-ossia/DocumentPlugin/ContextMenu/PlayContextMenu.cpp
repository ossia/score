#include "PlayContextMenu.hpp"
#include "source/Control/ScenarioControl.hpp"
#include "DocumentPlugin/OSSIAStateElement.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/State/StateModel.hpp"
#include "DocumentPlugin/OSSIAScenarioElement.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <API/Headers/Editor/State.h>
#include <API/Headers/Editor/TimeConstraint.h>
#include <core/document/DocumentModel.hpp>
#include "DocumentPlugin/OSSIADocumentPlugin.hpp"
PlayContextMenu::PlayContextMenu(ScenarioControl *parent):
    AbstractMenuActions(iscore::ToplevelMenuElement::AboutMenu, parent)
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
                s_plugin->states().at(state->id())->rootState()->launch();
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
                s_plugin->events().at(ev->id())->event()->happen();
            }
        }
    });

}

void PlayContextMenu::fillMenuBar(iscore::MenubarManager *menu)
{

}

void PlayContextMenu::fillContextMenu(QMenu *menu, const Selection & s)
{
    if(std::any_of(s.cbegin(), s.cend(), [] (auto obj) { return dynamic_cast<const StateModel*>(obj);}))
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

void PlayContextMenu::makeToolBar(QToolBar *)
{
    // nothing to do
}

void PlayContextMenu::setEnabled(bool b)
{
    m_playStates->setEnabled(b);
}
