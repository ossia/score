#pragma once
#include "source/Control/Menus/ScenarioActions.hpp"
class PlayContextMenu : public ScenarioActions
{
    public:
        PlayContextMenu(ScenarioControl* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, LayerPresenter* pres, const QPoint&, const QPointF&) override;

        void setEnabled(bool) override;


    private:
        QAction* m_recordAction{};

        QAction *m_playStates{};
        QAction *m_playEvents{};
        QAction *m_playConstraints{};
};
