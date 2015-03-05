#include "DurationSectionWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

#include "Commands/Constraint/SetMinDuration.hpp"
#include "Commands/Constraint/SetMaxDuration.hpp"
#include "Commands/Scenario/ResizeConstraint.hpp"
#include "Commands/ResizeBaseConstraint.hpp"
#include "Commands/Constraint/SetRigidity.hpp"

#include "core/interface/document/DocumentInterface.hpp"
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QGridLayout>
#include <QTimeEdit>

DurationSectionWidget::DurationSectionWidget(ConstraintInspectorWidget* parent) :
    InspectorSectionWidget {"Durations", parent},
    m_model {parent->model()},
    m_parent {parent},
    m_cmdManager{new OngoingCommandDispatcher{iscore::IDocument::commandQueue(iscore::IDocument::documentFromObject(m_model)),
                                           this}}
{

    QWidget* widg{new QWidget{this}};
    QGridLayout* lay = new QGridLayout{widg};
    lay->setContentsMargins(0, 0, 0 , 0);
    widg->setLayout(lay);

    m_valueLabel = new QLabel{"Default: "};
    auto checkbox = new QCheckBox{"Rigid"};
    auto minSpin = new QTimeEdit{};
    auto maxSpin = new QTimeEdit{};
    auto valueSpin = new QTimeEdit{};

    // TODO these need to be updated when the default duration changes
    connect(m_model, &ConstraintModel::defaultDurationChanged,
            this,	 &DurationSectionWidget::on_defaultDurationChanged);

    valueSpin->setDisplayFormat(QString("mm.ss.zzz"));
    minSpin->setDisplayFormat(QString("mm.ss.zzz"));
    maxSpin->setDisplayFormat(QString("mm.ss.zzz"));

    QTime msDuration = QTime(0,0,0,0).addMSecs(m_model->defaultDuration().msec());

//    minSpin->setMinimumTime(QTime(0,0,0,0));
    minSpin->setMaximumTime(msDuration);
    maxSpin->setMinimumTime(msDuration.addMSecs(1));
//    maxSpin->setMaximumTime(std::numeric_limits<int>::max());

//    valueSpin->setMinimum(std::numeric_limits<int>::min());
//    valueSpin->setMaximum(std::numeric_limits<int>::max());

    connect(checkbox, &QCheckBox::toggled,
            [ = ](bool val)
    {
        minSpin->setEnabled(!val);
        maxSpin->setEnabled(!val);
    });

    minSpin->setTime(QTime(0,0,0,0).addMSecs(m_model->minDuration().msec()));
    maxSpin->setTime(QTime(0,0,0,0).addMSecs(m_model->maxDuration().msec()));

    if(m_model->minDuration() == m_model->maxDuration())
    {
        checkbox->setChecked(true);
    }

    valueSpin->setTime(msDuration);

    lay->addWidget(checkbox, 0, 0);
    lay->addWidget(new QLabel{tr("Min duration") }, 1, 0);
    lay->addWidget(minSpin, 1, 1);
    lay->addWidget(new QLabel{tr("Max duration") }, 2, 0);
    lay->addWidget(maxSpin, 2, 1);

    lay->addWidget(m_valueLabel, 3, 0);
    lay->addWidget(valueSpin, 3, 1);


    connect(minSpin,	&QTimeEdit::timeChanged,
            [=] (QTime val) { emit minDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(minSpin,	&QTimeEdit::editingFinished,
            [&]() { m_cmdManager->commit(); });

    connect(maxSpin,	&QTimeEdit::timeChanged,
            [=] (QTime val) { emit maxDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(maxSpin,	&QTimeEdit::editingFinished,
            [&]() { m_cmdManager->commit(); });

    connect(valueSpin,  &QTimeEdit::timeChanged,
            [=] (QTime val) { emit defaultDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(valueSpin,  &QTimeEdit::editingFinished,
            [&]() { m_cmdManager->commit(); });

    connect(checkbox,	&QCheckBox::toggled,
            this,		&DurationSectionWidget::rigidCheckboxToggled);


    addContent(widg);
}

using namespace Scenario::Command;

void DurationSectionWidget::minDurationSpinboxChanged(int val)
{
    auto cmd = new SetMinDuration(
                   iscore::IDocument::path(m_model),
                   std::chrono::milliseconds {val});

    m_cmdManager->send(cmd);
}

void DurationSectionWidget::maxDurationSpinboxChanged(int val)
{
    auto cmd = new SetMaxDuration(
                   iscore::IDocument::path(m_model),
                   std::chrono::milliseconds {val});

    m_cmdManager->send(cmd);
}

void DurationSectionWidget::defaultDurationSpinboxChanged(int val)
{
    iscore::SerializableCommand* cmd {};

    if(m_model->objectName() != "BaseConstraintModel")
    {
        cmd = new ResizeConstraint(
                  iscore::IDocument::path(m_model),
                  std::chrono::milliseconds {val});
    }
    else
    {
        cmd = new ResizeBaseConstraint(
                  iscore::IDocument::path(m_model),
                  std::chrono::milliseconds {val});
    }

    m_cmdManager->send(cmd);
}

void DurationSectionWidget::rigidCheckboxToggled(bool b)
{
    auto cmd = new SetRigidity(
                   iscore::IDocument::path(m_model),
                   b);

    emit m_parent->commandDispatcher()->send(cmd);
}

void DurationSectionWidget::on_defaultDurationChanged(TimeValue dur)
{
    //	m_valueLabel->setText(QString{"Default: %1 s"}.arg(dur));
}
