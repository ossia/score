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
#include <core/document/DocumentModel.hpp>
#include <interface/panel/PanelModelInterface.hpp>

#include <QApplication>

AutomationInspectorWidget::AutomationInspectorWidget (AutomationModel* automationModel,
													  QWidget* parent) :
	InspectorWidgetBase{nullptr},
	m_model{automationModel}
{
	setObjectName ("AutomationInspectorWidget");
	setParent(parent);

	std::vector<QWidget*> vec;

	auto widg = new QWidget;
	auto lay = new QHBoxLayout{widg};

	vec.push_back(widg);

	// LineEdit (QComplete it?)
	auto m_lineEdit = new QLineEdit;
	m_lineEdit->setText(m_model->address());
	connect(m_model, SIGNAL(addressChanged(QString)),
			m_lineEdit,	SLOT(setText(QString)));

	connect(m_lineEdit, &QLineEdit::editingFinished,
			[=] ()
	{
		on_addressChange(m_lineEdit->text());
	});

	lay->addWidget(m_lineEdit);

	// If there is a DeviceExplorer in the current document, use it
	// to make a widget.
	auto deviceexplorer = DeviceExplorer::getModel(automationModel);
	if(deviceexplorer)
	{
		// LineEdit completion
		auto completer = new DeviceCompleter{deviceexplorer, this};
		m_lineEdit->setCompleter(completer);

		// Menu button
		auto pb = new QPushButton{"/"};

		auto menuview = new QMenuView{pb};
		menuview->setModel(deviceexplorer);

		connect(menuview, &QMenuView::triggered,
				[=] (const QModelIndex& m)
		{
			auto addr = DeviceExplorer::addressFromModelIndex(m);

			m_lineEdit->setText(addr);
			on_addressChange(addr);
		} );

		pb->setMenu(menuview);

		lay->addWidget(pb);
	}

	updateSectionsView(static_cast<QVBoxLayout*>(layout()), vec);
}

// TODO validation
void AutomationInspectorWidget::on_addressChange(const QString& newText)
{
	if(newText != m_model->address())
	{
		auto cmd = new ChangeAddress{
				   ObjectPath::pathFromObject("BaseElementModel", m_model),
				   newText};

		submitCommand(cmd);
	}
}
