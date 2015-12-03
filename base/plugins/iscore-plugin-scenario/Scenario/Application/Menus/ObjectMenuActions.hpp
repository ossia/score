#pragma once
#include <QJsonObject>
#include <QList>
#include <QPoint>

#include <Process/ProcessFactoryKey.hpp>
#include "ScenarioActions.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/selection/Selection.hpp>

class AddProcessDialog;
class QAction;
class QMenu;
class QToolBar;
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
namespace Scenario {
struct Point;
}  // namespace Scenario
namespace iscore {
class MenubarManager;
}  // namespace iscore

class ObjectMenuActions final : public ScenarioActions
{
    public:
        ObjectMenuActions(iscore::ToplevelMenuElement, ScenarioApplicationPlugin* parent);
        void fillMenuBar(iscore::MenubarManager *menu) override;
        void fillContextMenu(QMenu* menu, const Selection&, const TemporalScenarioPresenter& pres, const QPoint&, const QPointF&) override;
        bool populateToolBar(QToolBar*) override;
        void setEnabled(bool) override;

        QList<QAction*> actions() const override;
    private:
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void pasteElements(const QJsonObject& obj, const Scenario::Point& origin);
        void writeJsonToSelectedElements(const QJsonObject &obj);
        void addProcessInConstraint(const ProcessFactoryKey&);
        void addTriggerToTimeNode();
        void removeTriggerFromTimeNode();

        CommandDispatcher<> dispatcher();


        QAction* m_removeElements{};
        QAction *m_clearElements{};
        QAction *m_copyContent{};
        QAction *m_cutContent{};
        QAction *m_pasteContent{};
        QAction *m_elementsToJson{};

        // TODO separate classes for these
        // Constraint
        QAction *m_addProcess{};
        QAction *m_interp{};

        // Event
        QAction *m_addTrigger{};
        QAction *m_removeTrigger{};

        // State
        QAction* m_updateStates{};

        AddProcessDialog* m_addProcessDialog{};
};
