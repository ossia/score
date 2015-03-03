#include "AutomationInspectorWidget.hpp"
#include "../Automation/AutomationModel.hpp"
#include <InspectorInterface/InspectorSectionWidget.hpp>
#include "../Commands/ChangeAddress.hpp"
#include "../device_explorer/DeviceInterface/DeviceExplorerInterface.hpp"
#include "../device_explorer/DeviceInterface/DeviceCompleter.hpp"

#include "../device_explorer/Panel/DeviceExplorerModel.hpp"
#include "../device_explorer/QMenuView/qmenuview.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <core/interface/document/DocumentInterface.hpp>
#include <core/document/DocumentModel.hpp>
#include <interface/panel/PanelModelInterface.hpp>


#include <QApplication>

AutomationInspectorWidget::AutomationInspectorWidget(AutomationModel* automationModel,
                                                     QWidget* parent) :
    InspectorWidgetBase {nullptr},
    m_model {automationModel}
{
    setObjectName("AutomationInspectorWidget");
    setParent(parent);

    QVector<QWidget*> vec;

    auto widg = new QWidget;
    auto vlay = new QVBoxLayout{widg};
    auto hlay = new QHBoxLayout{};

    vec.push_back(widg);

    // LineEdit (QComplete it?)
    auto m_lineEdit = new QLineEdit;
    m_lineEdit->setText(m_model->address());
    connect(m_model, SIGNAL(addressChanged(QString)),
            m_lineEdit,	SLOT(setText(QString)));

    connect(m_lineEdit, &QLineEdit::editingFinished,
            [ = ]()
    {
        on_addressChange(m_lineEdit->text());
    });

    vlay->addWidget(m_lineEdit);

    // If there is a DeviceExplorer in the current document, use it
    // to make a widget.
    auto deviceexplorer = DeviceExplorer::getModel(automationModel);

    if(deviceexplorer)
    {
        // LineEdit completion
        auto completer = new DeviceCompleter {deviceexplorer, this};
        m_lineEdit->setCompleter(completer);

        // Menu button
        auto pb = new QPushButton {"/"};

        auto menuview = new QMenuView {pb};
        menuview->setModel(deviceexplorer);

        connect(menuview, &QMenuView::triggered,
                [ = ](const QModelIndex & m)
        {
            auto addr = DeviceExplorer::addressFromModelIndex(m);

            m_lineEdit->setText(addr);
            on_addressChange(addr);
        });

        pb->setMenu(menuview);

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

        commandDispatcher()->send(cmd);
    }
}
