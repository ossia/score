#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/ProcessFactory.hpp>
#include <unordered_map>

class ConstraintModel;
class TemporalConstraintViewModel;
class ConstraintViewModel;
class RackModel;
class SlotModel;
class ScenarioModel;
class Process;
class TriggerInspectorWidget;
class InspectorWidgetList;
class DynamicProcessList;
class RackWidget;
class RackInspectorSection;
class QFormLayout;
class MetadataWidget;

/*!
 * \brief The ConstraintInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an Constraint (Timerack) element.
 */
class ConstraintInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit ConstraintInspectorWidget(
                const InspectorWidgetList& list,
                const DynamicProcessList& pl,
                const ConstraintModel& object,
                iscore::Document& doc,
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

        QWidget* makeStatesWidget(ScenarioModel*);

        const InspectorWidgetList& m_widgetList;
        const DynamicProcessList& m_processList;
        const ConstraintModel& m_model;
        QVector<QMetaObject::Connection> m_connections;

        InspectorSectionWidget* m_eventsSection {};
        InspectorSectionWidget* m_durationSection {};

        InspectorSectionWidget* m_processSection {};
        std::vector<InspectorSectionWidget*> m_processesSectionWidgets;

        TriggerInspectorWidget* m_triggerLine{};

        InspectorSectionWidget* m_rackSection {};
        RackWidget* m_rackWidget {};
        std::unordered_map<Id<RackModel>, RackInspectorSection*, id_hash<RackModel>> m_rackesSectionWidgets;

        std::list<QWidget*> m_properties;

        MetadataWidget* m_metadata {};

};
