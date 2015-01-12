#include "DurationSectionWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

#include "Commands/Constraint/SetMinDuration.hpp"
#include "Commands/Constraint/SetMaxDuration.hpp"
#include "Commands/Constraint/SetRigidity.hpp"

#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QGridLayout>


DurationSectionWidget::DurationSectionWidget(ConstraintInspectorWidget* parent):
	InspectorSectionWidget{"Durations", parent},
	m_model{parent->model()},
	m_parent{parent}
{
	QWidget* widg{new QWidget{this}};
	QGridLayout* lay = new QGridLayout{widg};
	lay->setContentsMargins(0, 0, 0 ,0);
	widg->setLayout(lay);

	auto checkbox = new QCheckBox{"Rigid"};
	auto minSpin = new QSpinBox{};
	auto maxSpin = new QSpinBox{};

	// TODO these need to be updated when the default duration changes
	minSpin->setMinimum(0);
	minSpin->setMaximum(m_model->defaultDuration());
	maxSpin->setMinimum(m_model->defaultDuration() + 1);
	maxSpin->setMaximum(std::numeric_limits<int>::max());

	connect(checkbox, &QCheckBox::toggled,
			[=] (bool val)
	{
		minSpin->setEnabled(!val);
		maxSpin->setEnabled(!val);
	});

	minSpin->setValue(m_model->minDuration());
	maxSpin->setValue(m_model->maxDuration());
	if(m_model->minDuration() == m_model->maxDuration())
	{
		checkbox->setChecked(true);
	}

	lay->addWidget(checkbox, 0, 0);
	lay->addWidget(new QLabel{tr("Min duration")}, 1, 0);
	lay->addWidget(minSpin, 1, 1);
	lay->addWidget(new QLabel{tr("Max duration")}, 2, 0);
	lay->addWidget(maxSpin, 2, 1);

	connect(minSpin,	SIGNAL(valueChanged(int)),
			this,		SLOT(minDurationSpinboxChanged(int)));
	connect(minSpin,	&QSpinBox::editingFinished,
			[&] ()
	{
		emit m_parent->validateOngoingCommand();
		m_minSpinboxEditing = false;
	});
	connect(maxSpin,	SIGNAL(valueChanged(int)),
			this,		SLOT(maxDurationSpinboxChanged(int)));
	connect(maxSpin,	&QSpinBox::editingFinished,
			[&] ()
	{
		emit m_parent->validateOngoingCommand();
		m_maxSpinboxEditing = false;
	});
	connect(checkbox,	&QCheckBox::toggled,
			this,		&DurationSectionWidget::rigidCheckboxToggled);

	addContent(widg);
}

using namespace Scenario::Command;

void DurationSectionWidget::minDurationSpinboxChanged(int val)
{
	auto cmd = new SetMinDuration(
				ObjectPath::pathFromObject(
					"BaseConstraintModel",
					m_model),
				val);
	if(!m_minSpinboxEditing)
	{
		m_minSpinboxEditing = true;

		emit m_parent->initiateOngoingCommand(cmd, m_model->parent());
	}
	else
	{
		emit m_parent->continueOngoingCommand(cmd);
	}
}

void DurationSectionWidget::maxDurationSpinboxChanged(int val)
{
	auto cmd = new SetMaxDuration(
				ObjectPath::pathFromObject(
					"BaseConstraintModel",
					m_model),
				val);
	if(!m_maxSpinboxEditing)
	{
		m_maxSpinboxEditing = true;

		emit m_parent->initiateOngoingCommand(cmd, m_model->parent());
	}
	else
	{
		emit m_parent->continueOngoingCommand(cmd);
	}
}

void DurationSectionWidget::rigidCheckboxToggled(bool b)
{
	auto cmd = new SetRigidity(
				   ObjectPath::pathFromObject(
					   "BaseConstraintModel",
					   m_model),
				   b);

	emit m_parent->submitCommand(cmd);
}
