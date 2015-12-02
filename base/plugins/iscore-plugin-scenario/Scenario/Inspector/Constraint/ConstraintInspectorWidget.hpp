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

class ConstraintModel;
class ConstraintViewModel;
class ProcessList;
class InspectorSectionWidget;
class InspectorWidgetList;
class MetadataWidget;
class Process;
class QObject;
class QWidget;
class RackInspectorSection;
class RackModel;
class RackWidget;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario
namespace iscore {
class Document;
}  // namespace iscore

/*!
 * \brief The ConstraintInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an Constraint (Timerack) element.
 */
class ConstraintInspectorWidget final : public InspectorWidgetBase, public Nano::Observer
{
        Q_OBJECT
    public:
        explicit ConstraintInspectorWidget(
                const InspectorWidgetList& list,
                const ProcessList& pl,
                const ConstraintModel& object,
                std::unique_ptr<ConstraintInspectorDelegate> del,
                const iscore::DocumentContext& context,
                QWidget* parent = 0);

        const ConstraintModel& model() const;

    public slots:
        void updateDisplayedValues();

        // These methods ask for creation and the signals originate from other parts of the inspector
        void createProcess(const ProcessFactoryKey& processName);
        void createRack();
        void createLayerInNewSlot(QString processName);

        void activeRackChanged(QString rack, ConstraintViewModel* vm);

        // Interface of Constraint

        // These methods are used to display created things
        void displaySharedProcess(const Process&);
        void setupRack(const RackModel&);

        void ask_processNameChanged(const Process& p, QString s);

    private:
        void on_processCreated(const Process&);
        void on_processRemoved(const Process&);

        void on_rackCreated(const RackModel&);
        void on_rackRemoved(const RackModel&);

        void on_constraintViewModelCreated(const ConstraintViewModel&);
        void on_constraintViewModelRemoved(const QObject*);

        QWidget* makeStatesWidget(Scenario::ScenarioModel*);

        const InspectorWidgetList& m_widgetList;
        const ProcessList& m_processList;
        const ConstraintModel& m_model;

        InspectorSectionWidget* m_eventsSection {};
        InspectorSectionWidget* m_durationSection {};

        InspectorSectionWidget* m_processSection {};
        std::vector<InspectorSectionWidget*> m_processesSectionWidgets;

        InspectorSectionWidget* m_rackSection {};
        RackWidget* m_rackWidget {};
        std::unordered_map<Id<RackModel>, RackInspectorSection*, id_hash<RackModel>> m_rackesSectionWidgets;

        std::list<QWidget*> m_properties;

        MetadataWidget* m_metadata {};

        std::unique_ptr<ConstraintInspectorDelegate> m_delegate;

};
