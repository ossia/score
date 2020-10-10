#include "ChainProcess.hpp"

#include <Media/Commands/InsertEffect.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <cmath>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::ChainProcess)
namespace Media
{

ChainProcess::ChainProcess(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    const QString& name,
    const score::DocumentContext& ctx,
    QObject* parent)
    : ::Process::ProcessModel{duration, id, name, parent}, m_context{ctx}
{
}

ChainProcess::~ChainProcess()
{
  m_effects.clear();
}

void ChainProcess::insertEffect(Process::ProcessModel* eff, int pos)
{
  bool bad_effect = false;
  // Check that the effect order makes sense.
  const Process::Inlets& inlets = eff->inlets();
  const Process::Outlets& outlets = eff->outlets();
  if (inlets.empty() || outlets.empty())
  {
    bad_effect = true;
  }
  pos = std::clamp(pos, 0, int(m_effects.size()));

  if (!bad_effect)
  {
    if (pos == 0)
    {
      if (inlets[0]->type() != m_inlets[0]->type())
      {
        bad_effect = true;
      }
    }

    if (pos == int(m_effects.size()) - 1)
    {
      if (outlets[0]->type() != m_outlets[0]->type())
      {
        bad_effect = true;
      }
    }

    if (pos > 0)
    {
      if (inlets[0]->type() != m_effects.at_pos(pos - 1).outlets()[0]->type())
      {
        bad_effect = true;
      }
    }

    if (m_effects.size() > 0 && pos < (int)m_effects.size())
    {
      if (outlets[0]->type() != m_effects.at_pos(pos).inlets()[0]->type())
      {
        bad_effect = true;
      }
    }
  }

  m_effects.insert_at(pos, eff);

  setBadChaining(bad_effect);
}

void ChainProcess::removeEffect(const Id<Process::ProcessModel>& e)
{
  m_effects.remove(e);
  checkChaining();
  // TODO adjust and check ports
  // TODO introduce a dummy effect if the ports don't match
}

void ChainProcess::moveEffect(const Id<Process::ProcessModel>& e, int new_pos)
{
  if (m_effects.size() == 0)
    new_pos = 0;
  else
    new_pos = std::clamp(new_pos, 0, (int)m_effects.size());

  auto old_pos = effectPosition(e);
  if (old_pos != -1)
  {
    m_effects.move(e, new_pos);
  }
  checkChaining();
}

int ChainProcess::effectPosition(const Id<Process::ProcessModel>& e) const
{
  return (int)m_effects.index(e);
}

void ChainProcess::checkChaining()
{
  int pos = 0;
  bool bad_effect = false;
  for (auto& eff : m_effects)
  {
    const Process::Inlets& inlets = eff.inlets();
    const Process::Outlets& outlets = eff.outlets();

    if (inlets.empty() || outlets.empty())
    {
      bad_effect = true;
      break;
    }

    if (pos == 0)
    {
      if (inlets[0]->type() != m_inlets[0]->type())
      {
        bad_effect = true;
        break;
      }
    }

    if (pos == int(m_effects.size()) - 1)
    {
      if (outlets[0]->type() != m_outlets[0]->type())
      {
        bad_effect = true;
        break;
      }
    }

    if (pos > 0)
    {
      auto& prev_outlets = m_effects.at_pos(pos - 1).outlets();
      if (!prev_outlets.empty())
      {
        if (inlets[0]->type() != prev_outlets[0]->type())
        {
          bad_effect = true;
          break;
        }
      }
    }
    if (m_effects.size() > 0 && (pos + 1) < (int)m_effects.size())
    {
      auto& next_inlets = m_effects.at_pos(pos + 1).inlets();
      if (!next_inlets.empty())
      {
        if (outlets[0]->type() != next_inlets[0]->type())
        {
          bad_effect = true;
          break;
        }
      }
    }

    pos++;
  }

  setBadChaining(bad_effect);
}

Selection ChainProcess::selectableChildren() const noexcept
{
  Selection s;
  for (auto& c : effects())
    s.append(&c);
  return s;
}

Selection ChainProcess::selectedChildren() const noexcept
{
  Selection s;
  for (auto& c : effects())
    if (c.selection.get())
      s.append(&c);
  return s;
}

void ChainProcess::setSelection(const Selection& s) const noexcept
{
  for (auto& c : effects())
  {
    c.selection.set(s.contains(&c));
  }
}

bool EffectRemover::remove(const Selection& s, const score::DocumentContext& ctx)
{
  if (s.size() == 1)
  {
    auto first = s.begin()->data();
    if (auto proc = qobject_cast<const Process::ProcessModel*>(first))
    {
      auto p = proc->parent();
      if (auto fxc = qobject_cast<ChainProcess*>(p))
      {
        CommandDispatcher<>{ctx.commandStack}.submit<RemoveEffect>(*fxc, *proc);
      }
    }
  }
  return false;
}
}
