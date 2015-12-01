#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <list>

class InspectorSectionWidget;
class QWidget;
class StateModel;
namespace iscore {
struct DocumentContext;
}  // namespace iscore

class StateInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit StateInspectorWidget(
                const StateModel& object,
                const iscore::DocumentContext& context,
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
