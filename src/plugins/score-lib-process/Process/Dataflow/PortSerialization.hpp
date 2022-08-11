#pragma once
#include <Process/Dataflow/Port.hpp>

namespace Process
{

// Unchecked, low-level functions (directly load from the factory)

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Inlet> load_inlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Inlet> load_inlet(JSONWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Outlet> load_outlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<Outlet> load_outlet(JSONWriter& wr, QObject* parent);

// Checked functions: will return a port of the correct type if it the
// actual save data differs
// (This is to allow upgrading from the old v2.5 save files
// which did not have distinct port types)

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<ValueInlet> load_value_inlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<ValueInlet> load_value_inlet(JSONWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<ValueOutlet> load_value_outlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<ValueOutlet> load_value_outlet(JSONWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<ControlInlet> load_control_inlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<ControlInlet> load_control_inlet(JSONWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<ControlOutlet>
load_control_outlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<ControlOutlet> load_control_outlet(JSONWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<AudioInlet> load_audio_inlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<AudioInlet> load_audio_inlet(JSONWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<AudioOutlet> load_audio_outlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<AudioOutlet> load_audio_outlet(JSONWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<MidiInlet> load_midi_inlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<MidiInlet> load_midi_inlet(JSONWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<MidiOutlet> load_midi_outlet(DataStreamWriter& wr, QObject* parent);

SCORE_LIB_PROCESS_EXPORT
std::unique_ptr<MidiOutlet> load_midi_outlet(JSONWriter& wr, QObject* parent);
}
