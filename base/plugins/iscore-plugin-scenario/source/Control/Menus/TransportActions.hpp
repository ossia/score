#pragma once
#include "Control/Menus/AbstractMenuActions.hpp"

class QToolBar;
class TransportActions : public ScenarioActions
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
                LayerPresenter* pres,
                const QPoint&, const QPointF&) override;

        void makeToolBar(
                QToolBar* bar) override;

        void setEnabled(bool) override;

        QList<QAction*> actions() const override;

    private:
        QAction* m_play{};
        QAction* m_stop{};
        QAction* m_goToStart{};
        QAction* m_goToEnd{};
        QAction* m_record{};
};
