#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>

#include <nano_signal_slot.hpp>
#include <QString>
#include <QVector>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include <Process/ProcessFactoryKey.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Inspector
{
class InspectorSectionWidget;
class InspectorWidgetList;
}
namespace Process
{
class ProcessModel;
class ProcessList;
}
class QObject;
class QWidget;

namespace iscore {
class Document;
}

namespace Scenario
{
class MetadataWidget;
class ConstraintModel;
class ConstraintViewModel;
class RackModel;
class ScenarioModel;
class ProcessTabWidget;
class ProcessViewTabWidget;

/*!
 * \brief The ConstraintInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an Constraint (Timerack) element.
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintInspectorWidget final : public Inspector::InspectorWidgetBase, public Nano::Observer
{
    public:
        explicit ConstraintInspectorWidget(
                const Inspector::InspectorWidgetList& list,
                const Process::ProcessList& pl,
                const ConstraintModel& object,
                std::unique_ptr<ConstraintInspectorDelegate> del,
                const iscore::DocumentContext& context,
                QWidget* parent = 0);

        ~ConstraintInspectorWidget();

        const ConstraintModel& model() const;

        const Inspector::InspectorWidgetList& widgetList() const
        { return m_widgetList; }

         const Process::ProcessList& processList() const { return m_processList; }

    private:
        QString tabName() override;

        void updateDisplayedValues();

        // Interface of Constraint

        // These methods are used to display created things

        void on_processCreated(const Process::ProcessModel&);
        void on_processRemoved(const Process::ProcessModel&);

        void on_constraintViewModelCreated(const ConstraintViewModel&);
        void on_constraintViewModelRemoved(const QObject*);

        QWidget* makeStatesWidget(Scenario::ScenarioModel*);

        const Inspector::InspectorWidgetList& m_widgetList;
        const Process::ProcessList& m_processList;
        const ConstraintModel& m_model;

        //InspectorSectionWidget* m_eventsSection {};
        Inspector::InspectorSectionWidget* m_durationSection {};

        Scenario::ProcessTabWidget* m_processesTabPage {};
        Scenario::ProcessViewTabWidget* m_viewTabPage{};

        std::list<QWidget*> m_properties;

        MetadataWidget* m_metadata {};

        std::unique_ptr<ConstraintInspectorDelegate> m_delegate;

};
}
