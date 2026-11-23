#pragma once
#include <Recording/Engine/ValueRecorder.hpp>
#include <Recording/Takes/RecordedNotes.hpp>
#include <Recording/Takes/WavStreamWriter.hpp>

#include <QString>

#include <score_plugin_recording_export.h>

#include <cstdint>
#include <vector>

namespace Recording
{
//! One pass of a take recorded over passes of the same length: box dates
struct Pass
{
  int64_t start{};
  int64_t end{};
};

/**
 * The passes of a take from `start` to `end` (box dates, flicks), each
 * `length` long from the start. A last pass shorter than the others is left
 * out, unless the take has no complete one.
 */
SCORE_PLUGIN_RECORDING_EXPORT
std::vector<Pass> passesOf(int64_t start, int64_t end, int64_t length);

/**
 * The frames [from, from + count) of a WAV file, written to another, turned
 * by `rotate` frames: the file starts with the last `rotate` frames of the
 * segment, then the others. Frames past the end of the source are silence.
 */
SCORE_PLUGIN_RECORDING_EXPORT
bool writeWavSegment(
    const QString& source, const QString& dest, int64_t from, int64_t count,
    int64_t rotate, WavStreamWriter::Format format);

//! The notes of a pass, dated from its start, turned by `rotate` (flicks)
//! round its length; a note is cut at the end of the pass
SCORE_PLUGIN_RECORDING_EXPORT
std::vector<RecordedNote>
notesOfPass(const std::vector<RecordedNote>& notes, Pass p, int64_t rotate);

//! The values of a pass, dated from its start, turned by `rotate` round its
//! length, in time order
SCORE_PLUGIN_RECORDING_EXPORT
std::vector<ValueTake::Event>
valuesOfPass(const std::vector<ValueTake::Event>& events, Pass p, int64_t rotate);
}
