#include "ProcessViewTabWidget.hpp"
#include "RackWidget.hpp"

#include <QVBoxLayout>
#include <Inspector/InspectorSectionWidget.hpp>

#include <Scenario/Inspector/Constraint/Widgets/Rack/RackInspectorSection.hpp>

#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>

#include <iscore/widgets/MarginLess.hpp>

namespace Scenario {

ProcessViewTabWidget::ProcessViewTabWidget(
        const ConstraintInspectorWidget& parentCstr, QWidget *parent) :
    QWidget(parent),
    m_constraintWidget{parentCstr},
    m_commandDispatcher{m_constraintWidget.commandDispatcher()}
{
    auto viewLay = new iscore::MarginLess<QVBoxLayout>{this};

    m_rackSection = new Inspector::InspectorSectionWidget {"Rackes", false, this};
    m_rackSection->setObjectName("Rackes");

    m_rackWidget = new RackWidget{this, this};

    viewLay->addWidget(m_rackSection);
    viewLay->addWidget(m_rackWidget);
    viewLay->addStretch(1);
}

void ProcessViewTabWidget::updateDisplayedValues()
{
    for(auto& rack_pair : m_rackesSectionWidgets)
    {
        m_rackSection->removeContent(rack_pair.second);
    }

    m_rackesSectionWidgets.clear();

    // Rack
    for(const auto& rack : m_constraintWidget.model().racks)
    {
        setupRack(rack);
    }
}

void ProcessViewTabWidget::activeRackChanged(QString rack, ConstraintViewModel* vm)
{
    // TODO mettre à jour l'inspecteur si la rack affichée change (i.e. via une commande réseau).
    if (m_rackWidget == nullptr)
        return;

    if(rack == m_rackWidget->hiddenText)
    {
        if(vm->isRackShown())
        {
            auto cmd = new Command::HideRackInViewModel(*vm);
            emit m_commandDispatcher->submitCommand(cmd);
        }
    }
    else
    {
        for (auto& r : m_constraintWidget.model().racks)
        {
            if(r.metadata.name() == rack)
            {
                auto id = r.id();
                auto cmd = new Command::ShowRackInViewModel(*vm, id);
                emit m_commandDispatcher->submitCommand(cmd);
            }
        }
    }
}

void ProcessViewTabWidget::createRack()
{
    auto cmd = new Command::AddRackToConstraint{m_constraintWidget.model()};
   m_commandDispatcher->submitCommand(cmd);
}

void ProcessViewTabWidget::setupRack(const RackModel& rack)
{
    // Display the widget
    RackInspectorSection* newRack = new RackInspectorSection {
                                    rack.metadata.name(),
                                    rack,
                                    m_constraintWidget,
                                    this};

    m_rackesSectionWidgets[rack.id()] = newRack;
    m_rackSection->addContent(newRack);
}

void ProcessViewTabWidget::on_rackCreated(const RackModel& rack)
{
    setupRack(rack);
    m_rackWidget->viewModelsChanged();
}

void ProcessViewTabWidget::on_rackRemoved(const RackModel& rack)
{
    auto ptr = m_rackesSectionWidgets[rack.id()];
    m_rackesSectionWidgets.erase(rack.id());

    if(ptr)
    {
        ptr->deleteLater();
    }

    m_rackWidget->viewModelsChanged();
}

}
