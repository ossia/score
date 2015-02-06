#include "AutomationInspectorWidget.hpp"
#include "../Automation/AutomationModel.hpp"
#include <InspectorInterface/InspectorSectionWidget.hpp>
#include "../Commands/ChangeAddress.hpp"

#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>

AutomationInspectorWidget::AutomationInspectorWidget (AutomationModel* object,
													  QWidget* parent) :
	InspectorWidgetBase{nullptr}
{
	setObjectName ("AutomationInspectorWidget");
	setParent(parent);

	std::vector<QWidget*> vec;

	auto le = new QLineEdit{this};
	le->setText(object->address());
	connect(object, SIGNAL(addressChanged(QString)),
			le,	SLOT(setText(QString)));

	connect(le, &QLineEdit::editingFinished,
			[=] ()
	{
		if(le->text() != object->address())
		{
			auto cmd = new ChangeAddress{
					   ObjectPath::pathFromObject("BaseElementModel", object),
					   le->text()};

			submitCommand(cmd);
		}
	});

	vec.push_back(le);

	updateSectionsView(static_cast<QVBoxLayout*>(layout()), vec);
}
