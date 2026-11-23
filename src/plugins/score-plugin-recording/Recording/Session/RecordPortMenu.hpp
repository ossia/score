#pragma once
#include <Process/Dataflow/PortContextMenu.hpp>

#include <score/plugins/Interface.hpp>

namespace Recording
{
/**
 * The recording entries of an audio or MIDI port's menu: record now, from the
 * next bar, a bar from the next bar, from the first note (MIDI), when
 * playback starts; or stop the recording of the document.
 */
class RecordPortMenu final : public Process::PortContextMenuFactory
{
  SCORE_CONCRETE("4d2b9e61-7c3a-4f8e-b5d0-2a6e9c1f7b34")
public:
  void setupMenu(QMenu& menu, const Process::Port& port, const Process::Context& ctx)
      override;
};
}
