#pragma once
#include <Process/Dataflow/PortContextMenu.hpp>

namespace Recording
{
//! The Record entries of the background of a node view: the processes
//! selected, or the addresses selected in the explorer, into its box
class RecordNodalMenu final : public Process::NodalViewMenuFactory
{
  SCORE_CONCRETE("8e4c1a72-3d5b-4f09-b6e2-7a9c0d14f3b8")
public:
  void setupMenu(QMenu& menu, QObject& interval, const Process::Context& ctx) override;
};
}
