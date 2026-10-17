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
  connect(menu.addAction(tr("Randomize")), &QAction::triggered, this, [this] {
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

void Controller::randomize()
{
  if(!hasPattern())
    return;
  auto p = current();
  Patternist::randomize(p, m_rng);
  if(status)
    status(tr("Randomized"));
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
