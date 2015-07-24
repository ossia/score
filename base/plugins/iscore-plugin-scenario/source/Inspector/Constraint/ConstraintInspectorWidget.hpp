#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <unordered_map>

class ConstraintModel;
class TemporalConstraintViewModel;
class ConstraintViewModel;
class RackModel;
class SlotModel;
class ScenarioModel;
class Process;

class RackWidget;
class RackInspectorSection;
class QFormLayout;
class MetadataWidget;

/*!
 * \brief The ConstraintInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an Constraint (Timerack) element.
 */
class ConstraintInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit ConstraintInspectorWidget(
                const ConstraintModel* object,
                QWidget* parent = 0);

        const ConstraintModel* model() const;

    public slots:
        void reloadDisplayedValues()
        {
            updateDisplayedValues(m_currentConstraint);
        }

        void updateDisplayedValues(const ConstraintModel* obj);

        // These methods ask for creation and the signals originate from other parts of the inspector
        void createProcess(QString processName);
        void createRack();
        void createLayerInNewSlot(QString processName);

        void activeRackChanged(QString rack, ConstraintViewModel* vm);

        // Interface of Constraint
        void on_processCreated(QString processName, id_type<Process> processId);
        void on_processRemoved(id_type<Process> processId);

        void on_rackCreated(id_type<RackModel> rackId);
        void on_rackRemoved(id_type<RackModel> rackId);

        void on_constraintViewModelCreated(id_type<ConstraintViewModel> cvmId);
        void on_constraintViewModelRemoved(id_type<ConstraintViewModel> cvmId);

        // These methods are used to display created things
        void displaySharedProcess(const Process&);
        void setupRack(const RackModel&);

    private:
        QWidget* makeStatesWidget(ScenarioModel*);
        const ConstraintModel* m_currentConstraint {};
        QVector<QMetaObject::Connection> m_connections;

        InspectorSectionWidget* m_eventsSection {};
        InspectorSectionWidget* m_durationSection {};

        InspectorSectionWidget* m_processSection {};
        std::vector<InspectorSectionWidget*> m_processesSectionWidgets;

        InspectorSectionWidget* m_rackSection {};
        RackWidget* m_rackWidget {};
        std::unordered_map<id_type<RackModel>, RackInspectorSection*, id_hash<RackModel>> m_rackesSectionWidgets;

        QVector<QWidget*> m_properties;

        MetadataWidget* m_metadata {};

};
