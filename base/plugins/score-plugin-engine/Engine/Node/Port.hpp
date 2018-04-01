#pragma once
#include <ossia/dataflow/safe_nodes/port.hpp>

namespace Control
{
using inlet_kind = ossia::safe_nodes::inlet_kind;
using outlet_kind = ossia::safe_nodes::outlet_kind;

using AddressInInfo = ossia::safe_nodes::address_in;
using AudioInInfo = ossia::safe_nodes::audio_in;
using AudioOutInfo = ossia::safe_nodes::audio_out;
using ValueInInfo = ossia::safe_nodes::value_in;
using ValueOutInfo = ossia::safe_nodes::value_out;
using MidiInInfo = ossia::safe_nodes::midi_in;
using MidiOutInfo = ossia::safe_nodes::midi_out;
using ControlInfo = ossia::safe_nodes::control_in;
}
