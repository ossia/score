#pragma once
#include <ossia/editor/scenario/time_value.hpp>
#include <Process/TimeValue.hpp>

namespace Engine::ossia_to_score
{
inline ::TimeVal defaultTime(ossia::time_value t)
{
  return t.infinite() ? ::TimeVal::infinite()
                      : ::TimeVal::fromMsecs(double(t) / 1000.);
}
}
