#pragma once
#include <Recording/Engine/ValueRecorder.hpp>

#include <QByteArray>

#include <optional>

#include <score_plugin_recording_export.h>

#include <vector>

namespace Recording
{
/**
 * A value take, in the format the loopers write and the Automation process
 * reads back ("score-value-take", see Fx/Looper/ValueTake.hpp): every value
 * at its frame since `start`, typed, and, when they are all numbers, vectors
 * or lists of numbers, how many numbers they have and their range, which is
 * what an automation can play of them. The dates of the events are in the
 * box's time, so the take keeps to the bars whatever the tempo was;
 * `tempo` is only noted in the file.
 */
SCORE_PLUGIN_RECORDING_EXPORT
QByteArray valueTakeJson(
    const std::vector<ValueTake::Event>& events, int64_t start, int64_t end, int rate,
    double tempo, int meterNumerator, int meterDenominator);

//! The numbers of a value an automation can play (numbers, booleans, vectors,
//! lists of numbers), or nothing
SCORE_PLUGIN_RECORDING_EXPORT
std::optional<std::vector<double>> numbersOf(const ossia::value& v);

//! How many numbers the values have when an automation can play them, else 0
SCORE_PLUGIN_RECORDING_EXPORT
int numericComponents(const std::vector<ValueTake::Event>& events);
}
