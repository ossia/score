#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/document/DocumentContext.hpp>

namespace Process
{
/**
 * @brief From a port, gives a name suitable for displaying in a process header.
 *
 * This is useful mainly for processes which are defined by their output,
 * e.g. the automations.
 */
SCORE_LIB_PROCESS_EXPORT
QString
displayNameForPort(const Process::Port& outlet, const score::DocumentContext& doc);
}
