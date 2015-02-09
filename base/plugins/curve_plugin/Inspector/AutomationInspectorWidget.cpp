#include "AutomationInspectorWidget.hpp"
#include "../Automation/AutomationModel.hpp"
#include <InspectorInterface/InspectorSectionWidget.hpp>
#include "../Commands/ChangeAddress.hpp"
#include "../device_explorer/DeviceInterface/DeviceList.hpp"
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

	auto m_lineEdit = new QLineEdit;
	m_lineEdit->setText(m_model->address());
	connect(m_model, SIGNAL(addressChanged(QString)),
			m_lineEdit,	SLOT(setText(QString)));

	connect(m_lineEdit, &QLineEdit::editingFinished,
			[=] ()
	{
		on_addressChange(m_lineEdit->text());
	});

	auto pb = new QPushButton{"/"};

	auto deviceexplorer = iscore::getDocumentFromObject(automationModel)
							->panel("DeviceExplorerPanelModel")
							->findChild<DeviceExplorerModel*>("DeviceExplorerModel");

	auto menuview = new QMenuView{pb};
	connect(menuview, &QMenuView::triggered,
			[=] (const QModelIndex& m)
	{
		QModelIndex index = m;
		QString txt;

		// Convert to address
		while(index.isValid())
		{
			txt.prepend(QString("/%1").arg(index.data(0).toString()));
			index = index.parent();
		}

		// Set in lineedit
		m_lineEdit->setText(txt);
		on_addressChange(txt);
	} );
	menuview->setModel(deviceexplorer);

	//pb->setMenu(rootNodeToQMenu());
	pb->setMenu(menuview);

	auto widg = new QWidget;
	auto lay = new QHBoxLayout{widg};
	lay->addWidget(m_lineEdit);
	lay->addWidget(pb);

	vec.push_back(widg);

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
