#pragma once
#include <Fx/Arpeggiator.hpp>

namespace Nodes::Arpeggiator::v2
{
struct Node
{
  halp_meta(name, "Arpeggiator")
  halp_meta(c_name, "Arpeggiator")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/midi-utilities.html#arpeggiator")
  halp_meta(description, "Arpeggiator")
  halp_meta(uuid, "4df48c1c-f5f2-4ab1-9f37-3b0adba139ae")

  // FIXME "note" bus instead of midi bus ; the host handles passing all the non note messages
  struct
  {
    halp::midi_bus<"in", libremidi::message> midi;
    Arpeggios arpeggios;
    halp::hslider_i32<"Octave", halp::irange{1, 7, 1}> octave;
    OctaveMode octave_mode;
    halp::hslider_i32<"Repeat", halp::irange{1, 8, 1}> repeat;
    //! Between two steps: seconds, or a note value, on the bars' grid.
    halp::time_chooser<"Rate", halp::range{0.01, 4., 0.125}> rate;
  } inputs;
  struct
  {
    halp::midi_out_bus<"out", libremidi::message> midi;
  } outputs;

  Engine engine;

  //! The grid's rate for the step (1 a whole note, 4 a quarter...): a
  //! synced step is a note value; a free one, its length at the tempo.
  double grid_rate(const halp::tick_musical& tk) const noexcept
  {
    const double secs = inputs.rate.value;
    if(!(secs > 0.) || !(tk.tempo > 0.))
      return 0.;
    return 240. / (secs * tk.tempo);
  }

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    engine.process(
        inputs.midi, outputs.midi, inputs.octave, inputs.octave_mode.value,
        inputs.repeat, inputs.arpeggios.value, tk, grid_rate(tk));
  }
};
}
