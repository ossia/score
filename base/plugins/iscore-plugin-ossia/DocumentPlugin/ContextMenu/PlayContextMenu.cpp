#include "PlayContextMenu.hpp"
#include "source/Control/ScenarioControl.hpp"
#include "DocumentPlugin/OSSIAStateElement.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/State/DisplayedStateModel.hpp"
#include "DocumentPlugin/OSSIAScenarioElement.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <API/Headers/Editor/State.h>
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

            for(const auto& state : selectedElements(sm->states()))
            {
                for(auto& ossia_state : s_plugin->states().at(state->id())->states())
                {
                    ossia_state.second->launch();
                }
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
}


QList<QAction *> PlayContextMenu::actions()
{
}
