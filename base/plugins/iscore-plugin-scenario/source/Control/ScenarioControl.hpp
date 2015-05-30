#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"
#include "Document/BaseElement/ProcessFocusManager.hpp"

class QActionGroup;
class ScenarioModel;
class ScenarioStateMachine;
class TemporalScenarioPresenter;

class EditMenuActions;

class ScenarioControl : public iscore::PluginControlInterface
{
    friend class EditMenuActions;

    public:
        ScenarioControl(iscore::Presenter* pres);

        virtual void populateMenus(iscore::MenubarManager*) override;
        virtual QList<OrderedToolbar> makeToolbars() override;

        virtual iscore::SerializableCommand* instantiateUndoCommand(const QString& name,
                const QByteArray& data) override;

        // TODO why not in the plug-in ? (cf. DeviceExplorer)
        // Or - why could not the control be instead the Plugin (what is its point) ?
        ProcessList* processList()
        { return &m_processList; }

    public slots:
        void createContextMenu(const QPoint &);

    protected:
        virtual void on_documentChanged() override;
        const ScenarioModel* focusedScenarioModel() const;

    private:
        ScenarioStateMachine& stateMachine() const;
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void writeJsonToSelectedElements(const QJsonObject &obj);

        ProcessList m_processList;
        QActionGroup* m_scenarioToolActionGroup{};
        QActionGroup* m_scenarioScaleModeActionGroup{};
        QActionGroup* m_shiftActionGroup{};
        QMetaObject::Connection m_focusConnection, m_defocusConnection;

        EditMenuActions* m_edit;

        QAction* m_selecttool{};

        TemporalScenarioPresenter* focusedPresenter() const;

        ProcessFocusManager* processFocusManager() const;
        void on_presenterFocused(ProcessPresenter* pvm);
        void on_presenterDefocused(ProcessPresenter* pvm);
};
