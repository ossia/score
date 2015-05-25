#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include "ProcessInterface/ProcessList.hpp"
#include "Document/BaseElement/ProcessFocusManager.hpp"

class QActionGroup;
class ScenarioModel;
class ScenarioStateMachine;
class TemporalScenarioViewModel;
class ScenarioControl : public iscore::PluginControlInterface
{
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

    protected:
        virtual void on_documentChanged() override;

    private:
        ScenarioStateMachine& stateMachine() const;
        QJsonObject copySelectedElementsToJson();
        QJsonObject cutSelectedElementsToJson();
        void writeJsonToSelectedElements(const QJsonObject &obj);

        ProcessList m_processList;
        QActionGroup* m_scenarioToolActionGroup{};
        QActionGroup* m_scenarioScaleModeActionGroup{};
        QMetaObject::Connection m_focusConnection, m_defocusConnection;

        QAction* m_selecttool{};

        QPointer<const TemporalScenarioViewModel> m_lastViewModel;

        const ScenarioModel* focusedScenarioModel() const;
        const TemporalScenarioViewModel* focusedScenarioViewModel() const;

        ProcessFocusManager* processFocusManager() const;
        void on_viewModelFocused(const ProcessViewModel* pvm);
        void on_viewModelDefocused(const ProcessViewModel* pvm);
};
