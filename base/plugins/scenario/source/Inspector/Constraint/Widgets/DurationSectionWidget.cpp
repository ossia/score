#include "DurationSectionWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

#include "Commands/Constraint/SetMinDuration.hpp"
#include "Commands/Constraint/SetMaxDuration.hpp"
#include "Commands/Scenario/ResizeConstraint.hpp"
#include "Commands/ResizeBaseConstraint.hpp"
#include "Commands/Constraint/SetRigidity.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <QCheckBox>
#include <QSpinBox>
#include <QToolButton>
#include <QLabel>
#include <QGridLayout>
#include <QTimeEdit>

using namespace iscore;
DurationSectionWidget::DurationSectionWidget(ConstraintInspectorWidget* parent) :
    InspectorSectionWidget {"Durations", parent},
    m_model {parent->model()},
    m_parent {parent},
    m_cmdManager{new OngoingCommandDispatcher<MergeStrategy::Simple>{
                            IDocument::documentFromObject(m_model)->commandStack(),
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

    QToolButton* timeValidation = new QToolButton{};
    timeValidation->setText("OK");

    m_default = m_model->defaultDuration();
    //QTime msDuration = m_default.toQTime();
    m_min = m_default;
    m_max = m_default;
    m_max.addMSecs(1);

//    minSpin->setMinimumTime(QTime(0,0,0,0));
    m_minSpin->setMaximumTime(m_min.toQTime());
    m_maxSpin->setMinimumTime(m_max.toQTime());
//    maxSpin->setMaximumTime(std::numeric_limits<int>::max());

//    valueSpin->setMinimum(std::numeric_limits<int>::min());
//    valueSpin->setMaximum(std::numeric_limits<int>::max());

    connect(checkbox, &QCheckBox::toggled,
            [ = ](bool val)
    {
        m_minSpin->setEnabled(!val);
        m_maxSpin->setEnabled(!val);
    });

    m_minSpin->setTime(m_min.toQTime());
    m_maxSpin->setTime((m_max.toQTime()));

    if(m_model->minDuration() == m_model->maxDuration())
    {
        checkbox->setChecked(true);
    }

    m_valueSpin->setTime(m_default.toQTime());

    lay->addWidget(checkbox, 0, 0);
    lay->addWidget(timeValidation, 0, 1);
    lay->addWidget(new QLabel{tr("Min duration") }, 1, 0);
    lay->addWidget(m_minSpin, 1, 1);
    lay->addWidget(new QLabel{tr("Max duration") }, 2, 0);
    lay->addWidget(m_maxSpin, 2, 1);

    lay->addWidget(m_valueLabel, 3, 0);
    lay->addWidget(m_valueSpin, 3, 1);

    // TODO bug/crashes si décommenté ... Problème avec les signaux utilisés ??
/*
    connect(m_minSpin,	&QTimeEdit::timeChanged,
            [=] (QTime val) { emit minDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(m_minSpin,	&QTimeEdit::editingFinished,
            [&]() { emit m_cmdManager->commit(); });

    connect(m_maxSpin,	&QTimeEdit::timeChanged,
            [=] (QTime val) { emit maxDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(m_maxSpin,	&QTimeEdit::editingFinished,
            [&]() { emit m_cmdManager->commit(); });

    connect(m_valueSpin,  &QTimeEdit::timeChanged,
            [=] (QTime val) { emit defaultDurationSpinboxChanged(val.msecsSinceStartOfDay()); });

    connect(m_valueSpin,  &QTimeEdit::editingFinished,
            [&]() { emit m_cmdManager->commit(); });
*/
    connect(timeValidation, &QToolButton::clicked,
            this,           &DurationSectionWidget::on_durationsChanged);

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

    emit m_cmdManager->submitCommand(cmd);
}

void DurationSectionWidget::maxDurationSpinboxChanged(int val)
{
    auto cmd = new SetMaxDuration(
                   iscore::IDocument::path(m_model),
                   std::chrono::milliseconds {val});

    emit m_cmdManager->submitCommand(cmd);
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

    emit m_cmdManager->submitCommand(cmd);
}

void DurationSectionWidget::rigidCheckboxToggled(bool b)
{
    auto cmd = new SetRigidity(
                   iscore::IDocument::path(m_model),
                   b);

    emit m_parent->commandDispatcher()->submitCommand(cmd);
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

void DurationSectionWidget::on_durationsChanged()
{
    if (m_default.toQTime() != m_valueSpin->time())
    {
        emit defaultDurationSpinboxChanged(m_valueSpin->time().msecsSinceStartOfDay());
        m_cmdManager->commit();
    }

    if (m_min.toQTime() != m_minSpin->time())
    {
        emit minDurationSpinboxChanged(m_minSpin->time().msecsSinceStartOfDay());
        m_cmdManager->commit();

    }if (m_max.toQTime() != m_maxSpin->time())
    {
        emit maxDurationSpinboxChanged(m_maxSpin->time().msecsSinceStartOfDay());
        m_cmdManager->commit();
    }
}
