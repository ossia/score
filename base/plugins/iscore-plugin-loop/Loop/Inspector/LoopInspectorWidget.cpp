#include "LoopInspectorWidget.hpp"
#include <Loop/LoopProcessModel.hpp>
#include <Inspector/InspectorSectionWidget.hpp>

#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <iscore/widgets/SpinBoxes.hpp>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QApplication>

LoopInspectorWidget::LoopInspectorWidget(
        const LoopProcessModel& LoopModel,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {LoopModel, doc, parent},
    m_model {LoopModel}
{
    /*
    setObjectName("LoopInspectorWidget");
    setParent(parent);

    std::list<QWidget*> props;

    QWidget* addProc = new QWidget(this);
    QHBoxLayout* addProcLayout = new QHBoxLayout;
    addProcLayout->setContentsMargins(0, 0, 0 , 0);
    addProc->setLayout(addProcLayout);

    QToolButton* addProcButton = new QToolButton;
    addProcButton->setText("+");
    addProcButton->setObjectName("addAProcess");

    auto addProcText = new QLabel("Add Process");
    addProcText->setStyleSheet(QString("text-align : left;"));

    addProcLayout->addWidget(addProcButton);
    addProcLayout->addWidget(addProcText);
    auto addProcess = new AddProcessDialog {m_processList, this};

    connect(addProcButton,  &QToolButton::pressed,
            addProcess, &AddProcessDialog::launchWindow);

    connect(addProcess, &AddProcessDialog::okPressed,
            this, &LoopInspectorWidget::createProcess);

    props.push_back(addProc);
*/
    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), {});
}
