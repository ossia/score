#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
class StateModel;
class QFormLayout;
class StateWidget;
class StateInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit StateInspectorWidget(
                const StateModel* object,
                QWidget* parent);

    public slots:
        void updateDisplayedValues(const StateModel* obj);

    private:
        QVector<QWidget*> m_properties;

        const StateModel* m_model{};

        InspectorSectionWidget* m_stateSection{};
        QVector<StateWidget*> m_stateWidgets;
};
