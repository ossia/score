#pragma once
#include <Recording/Engine/ValueRecorder.hpp>

#include <QByteArray>

#include <score_plugin_recording_export.h>

#include <vector>

namespace Recording
{
/**
 * A value take, in the format the loopers write and the Automation process
 * reads back ("score-value-take", see Fx/Looper/ValueTake.hpp): every value
 * at its frame since `start`, typed, and, when they are all numbers, vectors
 * or lists of numbers, how many numbers they have and their range, which is
 * what an automation can play of them.
 */
SCORE_PLUGIN_RECORDING_EXPORT
QByteArray valueTakeJson(
    const std::vector<ValueTake::Event>& events, int64_t start, int64_t end, int rate,
    double tempo, int meterNumerator, int meterDenominator);

//! How many numbers the values have when an automation can play them, else 0
SCORE_PLUGIN_RECORDING_EXPORT
int numericComponents(const std::vector<ValueTake::Event>& events);
}
