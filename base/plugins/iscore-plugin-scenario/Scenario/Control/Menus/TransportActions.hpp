#pragma once
#include <Scenario/Control/Menus/ScenarioActions.hpp>

class QToolBar;
class TransportActions final : public ScenarioActions
{
    public:
        TransportActions(
                iscore::ToplevelMenuElement menuElt,
                ScenarioControl* parent);

        void fillMenuBar(
                iscore::MenubarManager *menu) override;

        void fillContextMenu(
                QMenu* menu,
                const Selection&sel,
                const TemporalScenarioPresenter& pres,
                const QPoint&, const QPointF&) override;

        bool populateToolBar(
                QToolBar* bar) override;

        void setEnabled(bool) override;

        QList<QAction*> actions() const override;

        void stop();

    private:
        QAction* m_play{};
        QAction* m_stop{};
        QAction* m_goToStart{};
        QAction* m_goToEnd{};
        QAction* m_record{};
};
