#include "Commands.hpp"

#include <score/command/Command.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <Spline/Model.hpp>

namespace Spline
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Spline"};
  return key;
}

ChangeSpline::ChangeSpline(const ProcessModel& autom, const ossia::spline_data& newval)
    : m_path{autom}, m_old{autom.spline()}, m_new{newval}
{
}

void ChangeSpline::undo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setSpline(m_old);
}

void ChangeSpline::redo(const score::DocumentContext& ctx) const
{
  m_path.find(ctx).setSpline(m_new);
}

void ChangeSpline::update(const ProcessModel&, const ossia::spline_data& newval)
{
  m_new = newval;
}

void ChangeSpline::update(const ProcessModel&, ossia::spline_data&& newval)
{
  using namespace std;
  swap(m_new, newval);
}

void ChangeSpline::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old << m_new;
}

void ChangeSpline::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old >> m_new;
}

}
