#include "AddLayerModelWidget.hpp"

#include "SlotInspectorSection.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Process/Process.hpp>
#include <Process/LayerModel.hpp>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QInputDialog>

AddLayerModelWidget::AddLayerModelWidget(SlotInspectorSection* parentSlot) :
    QWidget {parentSlot}
{
    QHBoxLayout* layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0 , 0);
    this->setLayout(layout);

    // Button
    QToolButton* addButton = new QToolButton;
    addButton->setText("+");

    // Text
    auto addText = new QLabel("Add Process View");
    addText->setStyleSheet(QString("text-align : left;"));

    layout->addWidget(addButton);
    layout->addWidget(addText);

    connect(addButton, &QToolButton::pressed,
            [ = ]()
    {
        QStringList available_models;

        // 1. List the processes in the model.
        const auto& shared_process_list = parentSlot->model().parentConstraint().processes;

        // 2. List the processes that already have a view in this slot
        const auto& already_displayed_processes = parentSlot->model().layers;

        // 3. Compute the difference
        for(const auto& process : shared_process_list)
        {
            auto beg_it = already_displayed_processes.cbegin();
            auto end_it = already_displayed_processes.cend();
            auto it = std::find_if(beg_it,
                                   end_it,
                                   [&process](const LayerModel& lm)
            { return lm.processModel().id() == process.id(); });

            if(it == end_it)
            {
                available_models += QString::number(*process.id().val());
            }
        }

        // 4. Present a dialog with the availble id's
        if(available_models.size() > 0)
        {
            bool ok = false;
            auto process_name =
                    QInputDialog::getItem(
                        this,
                        QObject::tr("Choose a process id"),
                        QObject::tr("Choose a process id"),
                        available_models,
                        0,
                        false,
                        &ok);

            if(ok)
                parentSlot->createLayerModel(Id<Process> {process_name.toInt() });
        }
    });
}
