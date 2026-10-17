#include "PatternController.hpp"

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QMimeData>

#include <Patternist/Commands/PatternProperties.hpp>
#include <Patternist/PatternEdit.hpp>
#include <Patternist/PatternView.hpp>

namespace Patternist
{
Controller::Controller(
    const ProcessModel& model, const score::DocumentContext& ctx, QObject* parent)
    : QObject{parent}
    , m_model{model}
    , m_ctx{ctx}
{
}

void Controller::attach(View& view)
{
  connect(&view, &View::paintStep, this, [this](int lane, int step, Note n) {
    if(!hasPattern())
      return;
    auto p = current();
    if(setStep(p, lane, step, n))
      m_ctx.dispatcher.submit<UpdatePattern>(m_model, m_model.currentPattern(), p);
  });
  connect(&view, &View::paintFinished, this, [this] { m_ctx.dispatcher.commit(); });

  connect(&view, &View::noteChanged, this, [this](int lane, int note) {
    if(!hasPattern())
      return;
    auto p = current();
    if(!ossia::valid_index(lane, p.lanes))
      return;
    p.lanes[lane].note = note;
    m_ctx.dispatcher.submit<UpdatePattern>(m_model, m_model.currentPattern(), p);
  });
  connect(&view, &View::noteChangeFinished, this, [this] { m_ctx.dispatcher.commit(); });

  connect(
      &view, &View::fieldEdited, this, [this](int lane, int step, int field, float v) {
    if(!hasPattern())
      return;
    auto p = current();
    if(!ossia::valid_index(lane, p.lanes))
      return;
    auto& l = p.lanes[lane];
    if(!ossia::valid_index(step, l.params))
      return;
    StepParams sp = l.params[step];
    setStepField(sp, StepField(field), v);
    if(sp == l.params[step])
      return;
    l.params[step] = sp;
    m_ctx.dispatcher.submit<UpdatePattern>(m_model, m_model.currentPattern(), p);
  });
  connect(
      &view, &View::fieldValuesEdited, this, [this, &view](int lane, int field) {
    const auto& values = view.draggedValues();
    if(!hasPattern())
      return;
    auto p = current();
    if(!ossia::valid_index(lane, p.lanes))
      return;
    auto& l = p.lanes[lane];
    for(int i = 0; i < std::ssize(values) && i < std::ssize(l.params); i++)
      setStepField(l.params[i], StepField(field), values[i]);
    if(!(p == current()))
      m_ctx.dispatcher.submit<UpdatePattern>(m_model, m_model.currentPattern(), p);
  });
  connect(&view, &View::fieldEditFinished, this, [this] { m_ctx.dispatcher.commit(); });

  connect(&view, &View::laneValueEdited, this, [this](int lane, int which, int value) {
    if(!hasPattern())
      return;
    auto p = current();
    if(!ossia::valid_index(lane, p.lanes))
      return;
    (which == 0 ? p.lanes[lane].length : p.lanes[lane].division) = value;
    normalize(p);
    if(!(p == current()))
      m_ctx.dispatcher.submit<UpdatePattern>(m_model, m_model.currentPattern(), p);
  });
  connect(&view, &View::laneValueEditFinished, this, [this] {
    m_ctx.dispatcher.commit();
  });

  connect(&view, &View::muteToggled, this, &Controller::toggleMute);
  connect(&view, &View::soloToggled, this, &Controller::toggleSolo);
}

bool Controller::hasPattern() const noexcept
{
  return ossia::valid_index(m_model.currentPattern(), m_model.patterns());
}

Pattern Controller::current() const
{
  return m_model.patterns()[m_model.currentPattern()];
}

void Controller::submit(const Pattern& p)
{
  if(!hasPattern() || p == current())
  {
    if(status)
      status(tr("Nothing changed"));
    return;
  }
  CommandDispatcher<> disp{m_ctx.commandStack};
  disp.submit<UpdatePattern>(m_model, m_model.currentPattern(), p);
}

void Controller::fillMenu(QMenu& menu, int lane)
{
  if(!hasPattern())
    return;
  const auto& pat = m_model.patterns()[m_model.currentPattern()];

  if(ossia::valid_index(lane, pat.lanes))
  {
    auto mute = menu.addAction(tr("Mute lane"));
    mute->setCheckable(true);
    mute->setChecked(pat.lanes[lane].mute);
    connect(mute, &QAction::triggered, this, [this, lane] { toggleMute(lane); });

    auto solo = menu.addAction(tr("Solo lane"));
    solo->setCheckable(true);
    solo->setChecked(pat.lanes[lane].solo);
    connect(solo, &QAction::triggered, this, [this, lane] { toggleSolo(lane); });

    connect(
        menu.addAction(tr("Randomize lane")), &QAction::triggered, this,
        [this, lane] { randomize(lane); });

    auto remove = menu.addAction(tr("Remove lane"));
    remove->setEnabled(pat.lanes.size() > 1);
    connect(remove, &QAction::triggered, this, [this, lane] { removeLane(lane); });
    menu.addSeparator();
  }

  connect(
      menu.addAction(tr("Add lane")), &QAction::triggered, this, [this] { addLane(); });
  menu.addSeparator();
  connect(
      menu.addAction(tr("Copy pattern")), &QAction::triggered, this, [this] { copy(); });
  auto paste = menu.addAction(tr("Paste pattern"));
  if(auto md = QApplication::clipboard()->mimeData())
    paste->setEnabled(bool(patternFromMime(*md)));
  connect(paste, &QAction::triggered, this, [this] { this->paste(); });
  connect(menu.addAction(tr("Clear pattern")), &QAction::triggered, this, [this] {
    clear();
  });
  connect(menu.addAction(tr("Rotate left")), &QAction::triggered, this, [this] {
    rotate(-1);
  });
  connect(menu.addAction(tr("Rotate right")), &QAction::triggered, this, [this] {
    rotate(1);
  });
  connect(menu.addAction(tr("Randomize pattern")), &QAction::triggered, this, [this] {
    randomize();
  });
}

void Controller::copy() const
{
  if(!hasPattern())
    return;
  QApplication::clipboard()->setMimeData(patternToMime(current()));
  if(status)
    status(tr("Pattern %1 copied").arg(m_model.currentPattern()));
}

void Controller::paste()
{
  if(auto md = QApplication::clipboard()->mimeData())
  {
    if(auto p = patternFromMime(*md))
    {
      if(status)
        status(tr("Pasted into pattern %1").arg(m_model.currentPattern()));
      submit(*p);
      return;
    }
  }
  if(status)
    status(tr("The clipboard holds no pattern"));
}

void Controller::copySteps(const StepSelection& s) const
{
  if(!hasPattern() || !s.valid())
    return;
  QApplication::clipboard()->setMimeData(stepsToMime(Patternist::copySteps(current(), s)));
  if(status)
    status(tr("Steps of %n lane(s) copied", "", s.steps.size()));
}

bool Controller::pasteSteps(int lane, const std::vector<int>& starts)
{
  auto md = QApplication::clipboard()->mimeData();
  if(!md || !hasPattern())
    return false;
  auto steps = stepsFromMime(*md);
  if(!steps)
    return false;
  auto p = current();
  Patternist::pasteSteps(p, *steps, lane, starts);
  if(status)
    status(tr("Steps pasted"));
  submit(p);
  return true;
}

void Controller::clearSteps(const StepSelection& s)
{
  if(!hasPattern() || !s.valid())
    return;
  auto p = current();
  Patternist::clearSteps(p, s);
  submit(p);
}

void Controller::clear()
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::clear(p);
  if(status)
    status(tr("Pattern %1 cleared").arg(m_model.currentPattern()));
  submit(p);
}

void Controller::rotate(int steps)
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::rotate(p, steps);
  if(status)
    status(steps < 0 ? tr("Rotated left") : tr("Rotated right"));
  submit(p);
}

void Controller::randomize(int lane)
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::randomize(p, m_rng, lane);
  if(status)
    status(lane < 0 ? tr("Pattern randomized") : tr("Lane randomized"));
  submit(p);
}

void Controller::toggleMute(int lane)
{
  if(!hasPattern())
    return;
  auto p = current();
  if(!ossia::valid_index(lane, p.lanes))
    return;
  p.lanes[lane].mute = !p.lanes[lane].mute;
  submit(p);
}

void Controller::toggleSolo(int lane)
{
  if(!hasPattern())
    return;
  auto p = current();
  if(!ossia::valid_index(lane, p.lanes))
    return;
  p.lanes[lane].solo = !p.lanes[lane].solo;
  submit(p);
}

void Controller::addLane()
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::addLane(p);
  submit(p);
}

void Controller::removeLane(int lane)
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::removeLane(p, lane);
  submit(p);
}

void Controller::setLength(int length)
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::setLength(p, length);
  submit(p);
}

void Controller::setDivision(int division)
{
  if(!hasPattern() || division <= 0)
    return;
  auto p = current();
  p.division = division;
  submit(p);
}

void Controller::euclidean(int lane, int hits, int rotation, Combine c)
{
  editLane(lane, [&](Lane& l) {
    Patternist::euclidean(m_model.patterns()[m_model.currentPattern()], l, hits, rotation, c);
  });
  if(status)
    status(tr("%n hit(s) spread over the lane", "", hits));
}

void Controller::randomizeField(StepField f, int lane)
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::randomizeField(p, m_rng, f, lane);
  if(status)
    status(tr("%1 randomized").arg(fieldName(f)));
  submit(p);
}

void Controller::resetField(StepField f, int lane)
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::resetField(p, f, lane);
  if(status)
    status(tr("%1 reset").arg(fieldName(f)));
  submit(p);
}

void Controller::setCondition(int lane, int step, Condition c, int a, int b)
{
  editLane(lane, [&](Lane& l) {
    if(!ossia::valid_index(step, l.params))
      return;
    l.params[step].condition = c;
    l.params[step].cycleA = a;
    l.params[step].cycleB = b;
  });
}

void Controller::selectPattern(int index)
{
  if(!ossia::valid_index(index, m_model.patterns()) || index == m_model.currentPattern())
    return;
  CommandDispatcher<> disp{m_ctx.commandStack};
  disp.submit<SetCurrentPattern>(m_model, index);
}

void Controller::duplicatePattern()
{
  if(!hasPattern() || std::ssize(m_model.patterns()) >= ProcessModel::maxPatterns)
    return;
  auto list = m_model.patterns();
  const int idx = m_model.currentPattern() + 1;
  list.insert(list.begin() + idx, current());
  CommandDispatcher<> disp{m_ctx.commandStack};
  disp.submit<ReplacePatterns>(m_model, std::move(list), idx);
}

void Controller::removePattern()
{
  if(!hasPattern() || m_model.patterns().size() <= 1)
    return;
  auto list = m_model.patterns();
  const int idx = m_model.currentPattern();
  list.erase(list.begin() + idx);
  const int next = std::min(idx, int(list.size()) - 1);
  CommandDispatcher<> disp{m_ctx.commandStack};
  disp.submit<ReplacePatterns>(m_model, std::move(list), next);
}
}
