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
class RackInspectorSection;
class RackModel;
class RackWidget;
class ScenarioModel;

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

        void createRack();
        void activeRackChanged(QString rack, ConstraintViewModel* vm);

    private:
        QString tabName() override;

        void updateDisplayedValues();

        // These methods ask for creation and the signals originate from other parts of the inspector
        void createProcess(const ProcessFactoryKey& processName);
        void createLayerInNewSlot(QString processName);


        // Interface of Constraint

        // These methods are used to display created things
        void displaySharedProcess(const Process::ProcessModel&);
        void setupRack(const RackModel&);

        void ask_processNameChanged(const Process::ProcessModel& p, QString s);



        void on_processCreated(const Process::ProcessModel&);
        void on_processRemoved(const Process::ProcessModel&);

        void on_rackCreated(const RackModel&);
        void on_rackRemoved(const RackModel&);

        void on_constraintViewModelCreated(const ConstraintViewModel&);
        void on_constraintViewModelRemoved(const QObject*);

        QWidget* makeStatesWidget(Scenario::ScenarioModel*);

        const Inspector::InspectorWidgetList& m_widgetList;
        const Process::ProcessList& m_processList;
        const ConstraintModel& m_model;

        //InspectorSectionWidget* m_eventsSection {};
        Inspector::InspectorSectionWidget* m_durationSection {};

        Inspector::InspectorSectionWidget* m_processSection {};
        std::vector<Inspector::InspectorSectionWidget*> m_processesSectionWidgets;

        Inspector::InspectorSectionWidget* m_rackSection {};
        RackWidget* m_rackWidget {};
        std::unordered_map<Id<RackModel>, RackInspectorSection*, id_hash<RackModel>> m_rackesSectionWidgets;

        std::list<QWidget*> m_properties;

        MetadataWidget* m_metadata {};

        std::unique_ptr<ConstraintInspectorDelegate> m_delegate;

};
}
