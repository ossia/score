#pragma once

// Builds the clap_event_transport_t for a tick.
//
// CLAP song positions are *fixed-point*: clap_beattime / clap_sectime are
// int64 scaled by CLAP_BEATTIME_FACTOR / CLAP_SECTIME_FACTOR (1 << 31, see
// clap/fixedpoint.h). Passing raw beat counts makes every host-side position
// read as ~0 (beats / 2^31): plug-ins that follow the transport - step
// sequencers like Stochas, arpeggiators, tempo-synced delays - saw
// isPlaying==true but a song position frozen at zero and never advanced.

#include <ossia/dataflow/token_request.hpp>

#include <clap/all.h>

#include <cmath>

namespace Clap
{
inline clap_event_transport_t make_transport(const ossia::token_request& tk) noexcept
{
  // Quarter notes, like VST2 ppqPos / VST3 projectTimeMusic
  const double song_pos_beats = tk.musical_start_position;
  const double song_pos_seconds
      = tk.tempo > 0. ? song_pos_beats * (60. / tk.tempo) : 0.;

  uint32_t transport_flags
      = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE
        | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
  if(tk.prev_date != tk.date)
    transport_flags |= CLAP_TRANSPORT_IS_PLAYING;

  // Bar information
  const double bar_start = tk.musical_start_last_bar;
  const double quarters_per_bar = 4.0 * tk.signature.upper / tk.signature.lower;
  const int32_t bar_number = quarters_per_bar > 0.
                                 ? static_cast<int32_t>(bar_start / quarters_per_bar)
                                 : 0;

  return clap_event_transport_t{
      .header = {
          .size = sizeof(clap_event_transport_t),
          .time = 0,
          .space_id = CLAP_CORE_EVENT_SPACE_ID,
          .type = CLAP_EVENT_TRANSPORT,
          .flags = 0,
      },
      .flags = transport_flags,
      .song_pos_beats
      = (clap_beattime)std::round(song_pos_beats * CLAP_BEATTIME_FACTOR),
      .song_pos_seconds
      = (clap_sectime)std::round(song_pos_seconds * CLAP_SECTIME_FACTOR),
      .tempo = tk.tempo,
      .tempo_inc = 0.0,
      .loop_start_beats = 0,
      .loop_end_beats = 0,
      .loop_start_seconds = 0,
      .loop_end_seconds = 0,
      .bar_start = (clap_beattime)std::round(bar_start * CLAP_BEATTIME_FACTOR),
      .bar_number = bar_number,
      .tsig_num = static_cast<uint16_t>(tk.signature.upper),
      .tsig_denom = static_cast<uint16_t>(tk.signature.lower)};
}
}
