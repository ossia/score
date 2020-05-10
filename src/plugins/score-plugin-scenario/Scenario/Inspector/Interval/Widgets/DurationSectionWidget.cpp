// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DurationSectionWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/Interval/SetMaxDuration.hpp>
#include <Scenario/Commands/Interval/SetMinDuration.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorWidget.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SpinBoxes.hpp>
#include <score/widgets/TextLabel.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QString>
#include <QWidget>
#include <qnamespace.h>

#include <chrono>
namespace Scenario
{
/* TODO improve this
template<typename Create, typename Update>
class OngoingAbstractCommandDispatcher final : public ICommandDispatcher
{
public:
  OngoingAbstractCommandDispatcher(Create&& create, Update&& update, const
score::CommandStackFacade& stack) : ICommandDispatcher{stack} ,
m_create{create} , m_update{update}
  {
  }

  template<typename... Args>
  void submit(Args&&... args)
  {
    if (!m_cmd)
    {
      stack().disableActions();
      m_cmd.reset(m_create(args...));
      m_cmd->redo(stack().context());
    }
    else
    {
      update(*m_cmd);
      m_update(*m_cmd, std::forward<Args>(args)...);
      m_cmd->redo(stack().context());
    }
  }

  void commit()
  {
    if (m_cmd)
    {
      SendStrategy::Quiet::send(stack(), m_cmd.release());
      stack().enableActions();
    }
  }

  void rollback()
  {
    if (m_cmd)
    {
      m_cmd->undo(stack().context());
      stack().enableActions();
    }
    m_cmd.reset();
  }

private:
  Create m_create;
  Update m_update;
  std::unique_ptr<score::Command> m_cmd;
};
*/
class EditionGrid : public QWidget
{
public:
  EditionGrid(
      const IntervalModel& m,
      const score::DocumentContext& fac,
      QFormLayout& lay,
      const Scenario::EditionSettings& set)
      : m_model{m}
      , m_dur{m.duration}
      , m_editionSettings{set}
      , m_moveFactory{fac.app.interfaces<IntervalResizerList>().find(m)}
      , m_dispatcher{fac.commandStack}
      , m_simpleDispatcher{fac.commandStack}
  {
    using namespace score;
    auto editableGrid = &lay;
    editableGrid->setLabelAlignment(Qt::AlignRight);
    editableGrid->setFormAlignment(Qt::AlignRight);

    // SPINBOXES
    m_minSpin = new TimeSpinBox{this};
    m_maxSpin = new TimeSpinBox{this};
    m_valueSpin = new TimeSpinBox{this};
    m_valueSpin->setEnabled(bool(m_moveFactory));
    m_maxSpin->setTime(m_dur.maxDuration());
    m_minSpin->setTime(m_dur.minDuration());
    m_valueSpin->setTime(m_dur.defaultDuration());

    // CHECKBOXES
    m_minNonNullBox = new QCheckBox{tr("Min")};
    m_maxFiniteBox = new QCheckBox{tr("Max")};

    m_minNonNullBox->setChecked(!m_dur.isMinNull());
    m_maxFiniteBox->setChecked(!m_dur.isMaxInfinite());

    connect(
        m_minNonNullBox,
        &QCheckBox::toggled,
        this,
        &EditionGrid::on_minNonNullToggled);

    connect(
        m_maxFiniteBox,
        &QCheckBox::toggled,
        this,
        &EditionGrid::on_maxFiniteToggled);

    // DISPLAY
    m_minNull = new TextLabel{tr("Null")};
    m_minNull->hide();
    m_maxInfinity = new TextLabel{tr("Infinity")};
    m_maxInfinity->hide();

    editableGrid->addRow(tr("Duration"), m_valueSpin);

    auto minstack = new QStackedWidget;
    minstack->setSizePolicy(
        QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    minstack->addWidget(m_minNull);
    minstack->addWidget(m_minSpin);
    auto minboxwidg = new QWidget;
    auto minboxlay = new score::MarginLess<QHBoxLayout>{minboxwidg};
    minboxlay->addWidget(m_minNonNullBox);
    editableGrid->addRow(minboxwidg, minstack);

    auto maxstack = new QStackedWidget;
    maxstack->addWidget(m_maxInfinity);
    maxstack->addWidget(m_maxSpin);
    auto maxboxwidg = new QWidget;
    auto maxboxlay = new score::MarginLess<QHBoxLayout>{maxboxwidg};
    maxboxlay->addWidget(m_maxFiniteBox);
    editableGrid->addRow(maxboxwidg, maxstack);

    connect(
        m_valueSpin,
        &TimeSpinBox::editingFinished,
        this,
        &EditionGrid::on_durationsChanged);
    connect(
        m_minSpin,
        &TimeSpinBox::editingFinished,
        this,
        &EditionGrid::on_durationsChanged);
    connect(
        m_maxSpin,
        &TimeSpinBox::editingFinished,
        this,
        &EditionGrid::on_durationsChanged);

    m_min = m_model.duration.minDuration();
    m_max = m_model.duration.maxDuration();

    con(m_dur,
        &IntervalDurations::minNullChanged,
        this,
        &EditionGrid::on_modelMinNullChanged);
    con(m_dur,
        &IntervalDurations::maxInfiniteChanged,
        this,
        &EditionGrid::on_modelMaxInfiniteChanged);
    minstack->setCurrentIndex(m_model.duration.isMinNull() ? 0 : 1);
    maxstack->setCurrentIndex(m_model.duration.isMaxInfinite() ? 0 : 1);
    on_modelRigidityChanged(m_model.duration.isRigid());

    // We disable changing duration for the full view unless
    // it is the top-level one, because else
    // it may cause unexpected changes in parent scenarios.
    auto mod_del = dynamic_cast<Scenario::ScenarioDocumentModel*>(
        &fac.document.model().modelDelegate());
    if (isInFullView(m_model) && &m_model != &mod_del->baseInterval())
    {
      m_valueSpin->setEnabled(false);
    }
  }

  ~EditionGrid()
  {
    if (m_resizeCommand)
      delete m_resizeCommand;
  }
  void defaultDurationSpinboxChanged(ossia::time_value val)
  {
    if (m_moveFactory)
    {
      if (m_resizeCommand)
      {
        m_moveFactory->update(
            *m_resizeCommand, m_model, val);
        m_resizeCommand->redo(m_dispatcher.stack().context());
      }
      else
      {
        m_resizeCommand = m_moveFactory->make(
            m_model,
            val,
            m_editionSettings.expandMode(),
            LockMode::Free);
        if (m_resizeCommand)
          m_resizeCommand->redo(m_dispatcher.stack().context());
      }
    }
  }

  void on_modelRigidityChanged(bool b)
  {
<<<<<<< HEAD
    m_minTitle->setHidden(b);
    m_maxTitle->setHidden(b);
    if(b)
    {
      m_minNonNullBox->setHidden(b);
      m_minSpin->setHidden(b);

      m_maxSpin->setHidden(b);
      m_maxFiniteBox->setHidden(b);
    }
    else
    {
      on_modelMinNullChanged(m_dur.isMinNull());
      on_modelMaxInfiniteChanged(m_dur.isMaxInfinite());
    }
=======
    m_minNonNullBox->setHidden(b);
    m_minSpin->setHidden(b);

    m_maxSpin->setHidden(b);
    m_maxFiniteBox->setHidden(b);
>>>>>>> b921ee801... checkbox are smaller and the label is directly set
  }

  void on_modelMinNullChanged(bool b)
  {
    m_minNull->setVisible(b);
    {
      QSignalBlocker block{m_minNonNullBox};
      m_minNonNullBox->setChecked(!b);
    }
    m_minSpin->setVisible(!b);
  }

  void on_modelMaxInfiniteChanged(bool b)
  {
    m_maxInfinity->setVisible(b);
    {
      QSignalBlocker block{m_maxFiniteBox};
      m_maxFiniteBox->setChecked(!b);
    }
    m_maxSpin->setVisible(!b);
  }

  void on_modelDefaultDurationChanged(const TimeVal& dur)
  {
    if (dur == m_valueSpin->time())
      return;

    m_valueSpin->setTime(dur);
  }

  void on_modelMinDurationChanged(const TimeVal& dur)
  {
    if (dur == m_minSpin->time())
      return;

    m_minSpin->setTime(dur);
    m_min = dur;
  }

  void on_modelMaxDurationChanged(const TimeVal& dur)
  {
    if (dur == m_maxSpin->time())
      return;

    m_maxSpin->setTime(dur);
    m_max = dur;
  }

  void on_durationsChanged()
  {
    if (m_dur.defaultDuration() != m_valueSpin->time())
    {
      defaultDurationSpinboxChanged(m_valueSpin->time());
      if (m_resizeCommand)
        m_dispatcher.stack().push(m_resizeCommand);
      m_resizeCommand = nullptr;
    }

    if (m_dur.minDuration() != m_minSpin->time())
    {
      minDurationSpinboxChanged(m_minSpin->time());
      m_dispatcher.commit();
    }
    if (m_dur.maxDuration() != m_maxSpin->time())
    {
      maxDurationSpinboxChanged(m_maxSpin->time());
      m_dispatcher.commit();
    }
  }

  void on_minNonNullToggled(bool val)
  {
    m_minSpin->setVisible(val);
    m_minNull->setVisible(!val);

    m_min = !val ? m_dur.minDuration() : m_min;

    auto cmd = new Scenario::Command::SetMinDuration(m_model, m_min, !val);

    m_simpleDispatcher.submit(cmd);
  }

  void on_maxFiniteToggled(bool val)
  {
    m_maxSpin->setVisible(val);
    m_maxInfinity->setVisible(!val);

    m_max = !val ? m_dur.maxDuration() : m_dur.defaultDuration() * 1.2;

    auto cmd = new Scenario::Command::SetMaxDuration(m_model, m_max, !val);

    m_simpleDispatcher.submit(cmd);
  }

  void minDurationSpinboxChanged(ossia::time_value val)
  {
    using namespace Scenario::Command;
    m_dispatcher.submit<SetMinDuration>(
        m_model,
        val,
        !m_minNonNullBox->isChecked());
  }

  void maxDurationSpinboxChanged(ossia::time_value val)
  {
    using namespace Scenario::Command;
    m_dispatcher.submit<SetMaxDuration>(
        m_model,
        val,
        !m_maxFiniteBox->isChecked());
  }

  const IntervalModel& m_model;
  const IntervalDurations& m_dur;
  const Scenario::EditionSettings& m_editionSettings;
  IntervalResizer* m_moveFactory{};
  score::Command* m_resizeCommand{};

  score::TimeSpinBox* m_valueSpin{};

  QCheckBox* m_minNonNullBox{};
  QLabel* m_minNull{};
  score::TimeSpinBox* m_minSpin{};

  QCheckBox* m_maxFiniteBox{};
  QLabel* m_maxInfinity{};
  score::TimeSpinBox* m_maxSpin{};

  TimeVal m_max;
  TimeVal m_min;

  OngoingCommandDispatcher m_dispatcher;
  CommandDispatcher<> m_simpleDispatcher;
};
/*

class PlayGrid : public QWidget
{
public:
  PlayGrid(const IntervalDurations& dur) : m_dur{dur}
  {
    auto playingGrid = new score::MarginLess<QGridLayout>(this);

    m_minLab = new TextLabel{m_dur.minDuration().toString(), this};
    m_maxLab = new TextLabel{m_dur.maxDuration().toString(), this};
    m_defaultLab = new TextLabel{m_dur.defaultDuration().toString(), this};
    m_currentPosLab = new TextLabel{this};

    on_progress(m_dur.playPercentage());

    playingGrid->addWidget(new TextLabel(tr("Default Duration")), 0, 0, 1, 1);
    playingGrid->addWidget(m_defaultLab, 0, 1, 1, 1);
    playingGrid->addWidget(new TextLabel(tr("Min Duration")), 1, 0, 1, 1);
    playingGrid->addWidget(m_minLab, 1, 1, 1, 1);
    playingGrid->addWidget(new TextLabel(tr("Max Duration")), 2, 0, 1, 1);
    playingGrid->addWidget(m_maxLab, 2, 1, 1, 1);
    playingGrid->addWidget(new TextLabel{tr("Progress")}, 3, 0, 1, 1);
    playingGrid->addWidget(m_currentPosLab, 3, 1, 1, 1);

    con(m_dur, &IntervalDurations::playPercentageChanged, this,
        &PlayGrid::on_progress);
  }

  void on_progress(double p)
  {
    auto coeff = m_dur.isMaxInfinite() ? m_dur.defaultDuration().msec()
                                       : m_dur.maxDuration().msec();
    m_currentPosLab->setText(
        QString::number(p * coeff / 1000) + QString(" s"));
  }

  void on_modelRigidityChanged(bool b)
  {

    m_minLab->setHidden(b);
    m_maxLab->setHidden(b);
  }

  void on_modelDefaultDurationChanged(const TimeVal& dur)
  {
    m_defaultLab->setText(dur.toString());
  }

  void on_modelMinDurationChanged(const TimeVal& dur)
  {
    m_minLab->setText(dur.toString());
  }

  void on_modelMaxDurationChanged(const TimeVal& dur)
  {
    m_maxLab->setText(dur.toString());
  }

private:
  const IntervalDurations& m_dur;

  QLabel* m_maxLab{};
  QLabel* m_minLab{};
  QLabel* m_defaultLab{};
  QLabel* m_currentPosLab{};
};
*/
DurationWidget::DurationWidget(
    const Scenario::EditionSettings& set,
    QFormLayout& lay,
    IntervalInspectorWidget* parent)
    : QWidget{parent}
    , m_editingWidget{
          new EditionGrid{parent->model(), parent->context(), lay, set}}
// , m_playingWidget{new PlayGrid{parent->model().duration}}
{
  using namespace score;
  auto mainWidg = this;
  auto mainLay = new score::MarginLess<QHBoxLayout>{mainWidg};
  auto& model = parent->model();
  auto& dur = model.duration;

  // CONNECTIONS FROM MODEL
  // TODO these need to be updated when the default duration changes
  con(dur,
      &IntervalDurations::defaultDurationChanged,
      this,
      [](const TimeVal& t) {

      });
  con(dur, &IntervalDurations::minDurationChanged, this, [this](auto v) {
    // m_playingWidget->on_modelMinDurationChanged(v);
    m_editingWidget->on_modelMinDurationChanged(v);
  });
  con(dur, &IntervalDurations::maxDurationChanged, this, [this](auto v) {
    // m_playingWidget->on_modelMaxDurationChanged(v);
    m_editingWidget->on_modelMaxDurationChanged(v);
  });
  con(dur, &IntervalDurations::rigidityChanged, this, [this](auto v) {
    // m_playingWidget->on_modelRigidityChanged(v);
    m_editingWidget->on_modelRigidityChanged(v);
  });
  /*
    con(set, &EditionSettings::toolChanged, this, [=](Scenario::Tool t) {
      mainLay->setCurrentWidget(
          t == Tool::Playing ? (QWidget*)m_playingWidget
                             : (QWidget*)m_editingWidget);
    });
  */
  // mainLay->addWidget(m_playingWidget);
  mainLay->addWidget(m_editingWidget);
  /*
    mainLay->setCurrentWidget(
        set.tool() == Tool::Playing ? (QWidget*)m_playingWidget
                                    : (QWidget*)m_editingWidget);
  */
}
}
