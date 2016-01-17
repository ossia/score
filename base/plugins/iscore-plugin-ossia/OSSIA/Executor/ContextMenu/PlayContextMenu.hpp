#pragma once
#include <QPoint>

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/selection/Selection.hpp>

class QAction;
class QMenu;
namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
}
namespace iscore
{
class MenubarManager;
}  // namespace iscore

namespace RecreateOnPlay
{
class PlayContextMenu final : public Scenario::ScenarioActions
{
    public:
        PlayContextMenu(Scenario::ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, const Scenario::TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;

        void setEnabled(bool) override;

        const QAction& playFromHereAction() { return *m_playFromHere;}

    private:
        QAction* m_recordAction{};

        QAction *m_playStates{};
        QAction *m_playEvents{};
        QAction *m_playConstraints{};

        QAction* m_playFromHere{};
};
}
