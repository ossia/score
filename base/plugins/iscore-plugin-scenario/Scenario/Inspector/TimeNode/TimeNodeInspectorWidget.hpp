#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

#include <Scenario/Inspector/ExpressionValidator.hpp>
class TimeNodeModel;

class QFormLayout;
class MetadataWidget;
class EventShortCut;
class TriggerInspectorWidget;

/*!
 * \brief The TimeNodeInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an TimeNode (Timebox) element.
 */
class TimeNodeInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit TimeNodeInspectorWidget(
                const TimeNodeModel& object,
                iscore::Document& doc,
                QWidget* parent);
    signals:

    public slots:
        void updateDisplayedValues();

        void on_splitTimeNodeClicked();


    private:
        QVector<QWidget*> m_properties;
        std::vector<EventShortCut*> m_events;

        const TimeNodeModel& m_model;

        InspectorSectionWidget* m_eventList {};
        QLabel* m_date {};

        MetadataWidget* m_metadata {};

        TriggerInspectorWidget* m_trigwidg{};

};
