#pragma once
#include "source/Control/Menus/AbstractMenuActions.hpp"

class PlayContextMenu : public AbstractMenuActions
{
    public:
        PlayContextMenu(ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&) override;

    private:
        QAction* m_playStates{};
        // TODO QAction *m_playEvents{};
        // TODO QAction *m_playConstraints{};

};
