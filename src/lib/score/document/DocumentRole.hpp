#pragma once

namespace score
{
/**
 * @brief What a document is allowed to do to the machine it is open on.
 *
 * A score names hardware: sound cards, MIDI ports, cameras, OSC sockets,
 * render windows. Opening one has always meant claiming all of it, because the
 * machine holding the document was the machine running the show.
 *
 * That stops being true once a score can be edited from somewhere else. A
 * laptop driving a score that plays on a headless box must not open that box's
 * MIDI ports on itself, nor put its render window on the wrong screen -- and
 * the browser it might be a tab in has none of those things to offer anyway.
 *
 * Distinct from score::Environment, which answers where the *files* are: a
 * peer in a multiplayer session reads its score from another machine and still
 * plays it on its own hardware. Both answers are needed, and they differ.
 *
 * Known before the document is read rather than set afterwards: devices
 * connect while their plug-in deserializes, so anything decided later is
 * decided too late.
 */
enum class DocumentRole
{
  //! Ordinary. Devices connect, execution runs here, windows open here.
  Local,

  //! The score runs elsewhere. This copy is for editing and watching it: no
  //! ports, no hardware, no rendering, no executor.
  Terminal
};
}
