#include "AddBoxWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"


#include <QtWidgets/QGridLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QComboBox>

BoxWidget::BoxWidget(ConstraintInspectorWidget* parent):
	QWidget{parent},
	m_model{parent->model()}
{
	QGridLayout* lay = new QGridLayout{this};
	lay->setContentsMargins(0, 0, 0 ,0);
	this->setLayout(lay);

	// Button
	QToolButton* addButton = new QToolButton{this};
	addButton->setText ("+");

	// Text
	auto addText = new QLabel{"Add Box", this};
	addText->setStyleSheet(QString{"text-align : left;"});

	// Current box chooser
	m_boxList = new QComboBox{this};
	connect(m_boxList, &QComboBox::currentTextChanged,
			[=] (const QString& s) {
		parent->activeBoxChanged(s);
	});

	// Layout setup
	lay->addWidget(addButton, 0, 0);
	lay->addWidget(addText, 0, 1);
	lay->addWidget(m_boxList, 1, 0, 1, 2);

	connect(addButton, &QToolButton::pressed,
			[=] () { parent->createBox(); } );
}

void BoxWidget::updateComboBox()
{
	m_boxList->clear();
	m_boxList->addItem(hiddenText);
	if(m_model)
	{
		auto boxesPtrs = m_model->boxes();
		for(auto box : boxesPtrs)
		{
			m_boxList->addItem(QString::number(box->id()));
		}
	}
}

void BoxWidget::setModel(ConstraintModel* m)
{
	m_model = m;
}
