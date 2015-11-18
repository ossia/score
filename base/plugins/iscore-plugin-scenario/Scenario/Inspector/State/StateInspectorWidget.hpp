#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
class StateModel;
class QFormLayout;
class StateWidget;
class StateInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit StateInspectorWidget(
                const StateModel& object,
                iscore::Document& doc,
                QWidget* parent);

    public slots:
        void updateDisplayedValues();

    private slots:
        void splitEvent();

    private:
        std::list<QWidget*> m_properties;

        const StateModel& m_model;

        InspectorSectionWidget* m_stateSection{};
};
