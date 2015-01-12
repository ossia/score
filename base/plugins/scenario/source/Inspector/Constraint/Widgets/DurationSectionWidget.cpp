#include "DurationSectionWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QGridLayout>


DurationSectionWidget::DurationSectionWidget(ConstraintInspectorWidget* parent):
	InspectorSectionWidget{"Durations", parent},
	m_model{parent->model()}
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

	connect(minSpin, SIGNAL(valueChanged(int)),
			parent, SLOT(minDurationSpinboxChanged(int)));
	connect(maxSpin, SIGNAL(valueChanged(int)),
			parent, SLOT(maxDurationSpinboxChanged(int)));
	connect(checkbox, SIGNAL(toggled(bool)),
			parent, SLOT(rigidCheckboxToggled(bool)));

	addContent(widg);
}
