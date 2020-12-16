#include "Commands.hpp"
#include <Spline3D/Model.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Spline3D
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Spline3D"};
  return key;
}

ChangeSpline::ChangeSpline(const ProcessModel& autom, const ossia::spline3d_data& newval)
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

void ChangeSpline::update(const ProcessModel&, const ossia::spline3d_data& newval)
{
  m_new = newval;
}

void ChangeSpline::update(const ProcessModel&, ossia::spline3d_data&& newval)
{
  using namespace std;
  swap(m_new, newval);
}

void ChangeSpline::serializeImpl(DataStreamInput& s) const { s << m_path << m_old << m_new; }

void ChangeSpline::deserializeImpl(DataStreamOutput& s) { s >> m_path >> m_old >> m_new; }


}
