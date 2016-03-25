#pragma once

#include <QWidget>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>

namespace Inspector {
    class InspectorSectionWidget;
}


namespace Scenario {
class RackWidget;
class RackInspectorSection;

class ProcessViewTabWidget :
        public QWidget,
        public Nano::Observer
{
        Q_OBJECT
    public:
        explicit ProcessViewTabWidget(const ConstraintInspectorWidget& parentCstr, QWidget *parent = 0);

        void updateDisplayedValues();

        void activeRackChanged(Id<RackModel> rack, ConstraintViewModel* vm);
        void createRack();


        RackWidget* rackWidget()
        { return m_rackWidget; }
        const ConstraintInspectorWidget& parentConstraint()
        { return m_constraintWidget; }

    signals:

    public slots:

        void setupRack(const RackModel&);
        void on_rackCreated(const RackModel&);
        void on_rackRemoved(const RackModel&);

    private:

        const ConstraintInspectorWidget& m_constraintWidget;
        CommandDispatcher<>* m_commandDispatcher{};

        Inspector::InspectorSectionWidget* m_rackSection {};
        RackWidget* m_rackWidget {};
        std::unordered_map<Id<RackModel>, RackInspectorSection*, id_hash<RackModel>> m_rackesSectionWidgets;

};

}
