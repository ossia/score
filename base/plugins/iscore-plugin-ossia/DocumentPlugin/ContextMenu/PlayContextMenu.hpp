#pragma once
#include "Scenario/Application/Menus/ScenarioActions.hpp"
class PlayContextMenu final : public ScenarioActions
{
    public:
        PlayContextMenu(ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;

        void setEnabled(bool) override;

        const QAction& playFromHereAction() { return *m_playFromHere;}

    private:
        QAction* m_recordAction{};

        QAction *m_playStates{};
        QAction *m_playEvents{};
        QAction *m_playConstraints{};

        QAction* m_playFromHere{};
};
