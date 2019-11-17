#pragma once
#include <Process/TimeValue.hpp>

#include <ossia/editor/scenario/time_value.hpp>

namespace Engine::ossia_to_score
{
inline ::TimeVal defaultTime(ossia::time_value t)
{
  return t.infinite() ? ::TimeVal::infinite()
                      : ::TimeVal::fromMsecs(double(t.impl) / 1000.);
}
}
