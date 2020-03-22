#pragma once

#include <QPointer>
#include <Process/Dataflow/Port.hpp>
#include <ossia/network/value/value.hpp>

namespace Process
{
struct SCORE_LIB_PROCESS_EXPORT ControlMessage
{
  Path<Process::Inlet> port;
  ossia::value value;

  QString name(const score::DocumentContext& ctx) const noexcept;
};
}
