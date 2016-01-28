#pragma once
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <QList>
#include <QPoint>

#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>

class QAction;
class QMenu;
class QToolBar;
namespace iscore {
class MenubarManager;
}  // namespace iscore

namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class TransportActions final : public ScenarioActions
{
    public:
        TransportActions(
                iscore::ToplevelMenuElement menuElt,
                ScenarioApplicationPlugin* parent);

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
}
