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
    m_minSpin = new QTimeEdit{};
    m_maxSpin = new QTimeEdit{};
    m_valueSpin = new QTimeEdit{};

    // TODO these need to be updated when the default duration changes
    connect(m_model, &ConstraintModel::defaultDurationChanged,
            this,	 &DurationSectionWidget::on_modelDefaultDurationChanged);
    connect(m_model,    &ConstraintModel::minDurationChanged,
            this,       &DurationSectionWidget::on_modelMinDurationChanged);
    connect(m_model,    &ConstraintModel::maxDurationChanged,
            this,       &DurationSectionWidget::on_modelMaxDurationChanged);

    m_valueSpin->setDisplayFormat(QString("mm.ss.zzz"));
    m_minSpin->setDisplayFormat(QString("mm.ss.zzz"));
    m_maxSpin->setDisplayFormat(QString("mm.ss.zzz"));

    QTime msDuration = m_model->defaultDuration().toQTime();

//    minSpin->setMinimumTime(QTime(0,0,0,0));
    m_minSpin->setMaximumTime(msDuration);
    m_maxSpin->setMinimumTime(msDuration.addMSecs(1));
//    maxSpin->setMaximumTime(std::numeric_limits<int>::max());

//    valueSpin->setMinimum(std::numeric_limits<int>::min());
//    valueSpin->setMaximum(std::numeric_limits<int>::max());

    connect(checkbox, &QCheckBox::toggled,
            [ = ](bool val)
    {
        m_minSpin->setEnabled(!val);
        m_maxSpin->setEnabled(!val);
    });

    m_minSpin->setTime(m_model->minDuration().toQTime());
    m_maxSpin->setTime((m_model->maxDuration().toQTime()));

    if(m_model->minDuration() == m_model->maxDuration())
    {
        checkbox->setChecked(true);
    }

    m_valueSpin->setTime(msDuration);

    lay->addWidget(checkbox, 0, 0);
    lay->addWidget(new QLabel{tr("Min duration") }, 1, 0);
    lay->addWidget(m_minSpin, 1, 1);
    lay->addWidget(new QLabel{tr("Max duration") }, 2, 0);
    lay->addWidget(m_maxSpin, 2, 1);

    lay->addWidget(m_valueLabel, 3, 0);
    lay->addWidget(m_valueSpin, 3, 1);


    connect(m_minSpin,	&QTimeEdit::timeChanged,
            [=] (QTime val) { emit minDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(m_minSpin,	&QTimeEdit::editingFinished,
            [&]() { m_cmdManager->commit(); });

    connect(m_maxSpin,	&QTimeEdit::timeChanged,
            [=] (QTime val) { emit maxDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(m_maxSpin,	&QTimeEdit::editingFinished,
            [&]() { m_cmdManager->commit(); });

    connect(m_valueSpin,  &QTimeEdit::timeChanged,
            [=] (QTime val) { emit defaultDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(m_valueSpin,  &QTimeEdit::editingFinished,
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

void DurationSectionWidget::on_modelDefaultDurationChanged(TimeValue dur)
{
    if (dur.toQTime() == m_valueSpin->time())
        return;

    m_valueSpin->setTime(dur.toQTime());
}

void DurationSectionWidget::on_modelMinDurationChanged(TimeValue dur)
{
    if (dur.toQTime() == m_minSpin->time())
        return;

    m_minSpin->setTime(dur.toQTime());
}

void DurationSectionWidget::on_modelMaxDurationChanged(TimeValue dur)
{
    if (dur.toQTime() == m_maxSpin->time())
        return;

    m_maxSpin->setTime(dur.toQTime());
}
