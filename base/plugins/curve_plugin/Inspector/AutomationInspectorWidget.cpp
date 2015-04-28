#include "AutomationInspectorWidget.hpp"
#include "../Automation/AutomationModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include "../Commands/ChangeAddress.hpp"

#include <Singletons/DeviceExplorerInterface.hpp>
#include <DeviceExplorer/../Plugin/Widgets/DeviceCompleter.hpp>
#include <DeviceExplorer/../Plugin/Widgets/DeviceExplorerMenuButton.hpp>
#include <DeviceExplorer/../Plugin/Panel/DeviceExplorerModel.hpp>

#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>


#include <QApplication>

AutomationInspectorWidget::AutomationInspectorWidget(AutomationModel* automationModel,
                                                     QWidget* parent) :
    InspectorWidgetBase {automationModel, parent},
    m_model {automationModel}
{
    setObjectName("AutomationInspectorWidget");
    setParent(parent);

    QVector<QWidget*> vec;

    auto widg = new QWidget;
    auto vlay = new QVBoxLayout{widg};
    vlay->setSpacing(0);
    vlay->setContentsMargins(0,0,0,0);
    auto hlay = new QHBoxLayout{};
    hlay->setSpacing(0);
    hlay->setContentsMargins(0,0,0,0);

    vec.push_back(widg);

    // LineEdit
    m_lineEdit = new QLineEdit;
    m_lineEdit->setText(m_model->address());
    connect(m_model, SIGNAL(addressChanged(QString)),
            m_lineEdit,	SLOT(setText(QString)));

    connect(m_lineEdit, &QLineEdit::editingFinished,
            [=]() { on_addressChange(m_lineEdit->text()); });

    vlay->addWidget(m_lineEdit);

    // If there is a DeviceExplorer in the current document, use it
    // to make a widget.
    auto deviceexplorer = iscore::IDocument::documentFromObject(automationModel)->findChild<DeviceExplorerModel*>("DeviceExplorerModel");
    if(deviceexplorer)
    {
        // LineEdit completion
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_lineEdit->setCompleter(completer);

        auto pb = new DeviceExplorerMenuButton{deviceexplorer};
        connect(pb, &DeviceExplorerMenuButton::addressChosen,
                this, [&] (const QString& addr)
        {
            m_lineEdit->setText(addr);
            on_addressChange(addr);
        });

        hlay->addWidget(pb);
    }

    // Add it to a new deck
    auto display = new QPushButton{"~"};
    hlay->addWidget(display);
    connect(display,    &QPushButton::clicked,
            [ = ]()
    {
        createViewInNewDeck(QString::number(m_model->id_val()));
    });

    vlay->addLayout(hlay);


    updateSectionsView(static_cast<QVBoxLayout*>(layout()), vec);
}

// TODO validation (voir dans capacités de QLineEdit)
void AutomationInspectorWidget::on_addressChange(const QString& newText)
{
    if(newText != m_model->address())
    {
        auto cmd = new ChangeAddress{
                    iscore::IDocument::path(m_model),
                    newText };

        commandDispatcher()->submitCommand(cmd);
    }
}
