#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <unordered_map>

class ConstraintModel;
class TemporalConstraintViewModel;
class AbstractConstraintViewModel;
class BoxModel;
class SlotModel;
class ScenarioModel;
class ProcessModel;

class BoxWidget;
class BoxInspectorSection;
class QFormLayout;
class MetadataWidget;

/*!
 * \brief The ConstraintInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an Constraint (Timebox) element.
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
        void createBox();
        void createLayerInNewSlot(QString processName);

        void activeBoxChanged(QString box, AbstractConstraintViewModel* vm);

        // Interface of Constraint
        void on_processCreated(QString processName, id_type<ProcessModel> processId);
        void on_processRemoved(id_type<ProcessModel> processId);

        void on_boxCreated(id_type<BoxModel> boxId);
        void on_boxRemoved(id_type<BoxModel> boxId);

        void on_constraintViewModelCreated(id_type<AbstractConstraintViewModel> cvmId);
        void on_constraintViewModelRemoved(id_type<AbstractConstraintViewModel> cvmId);

        // These methods are used to display created things
        void displaySharedProcess(ProcessModel*);
        void setupBox(BoxModel*);

    private:
        QWidget* makeEventWidget(ScenarioModel*);
        const ConstraintModel* m_currentConstraint {};
        QVector<QMetaObject::Connection> m_connections;

        InspectorSectionWidget* m_eventsSection {};
        InspectorSectionWidget* m_durationSection {};

        InspectorSectionWidget* m_processSection {};
        std::vector<InspectorSectionWidget*> m_processesSectionWidgets;

        InspectorSectionWidget* m_boxSection {};
        BoxWidget* m_boxWidget {};
        std::unordered_map<id_type<BoxModel>, BoxInspectorSection*, id_hash<BoxModel>> m_boxesSectionWidgets;

        QVector<QWidget*> m_properties;

        MetadataWidget* m_metadata {};

};
