#pragma once

#include <ossia/detail/small_vector.hpp>

namespace Process
{
class Port;
class Inlet;
class ControlInlet;
class Outlet;
class AudioInlet;
class AudioOutlet;
class MidiInlet;
class MidiOutlet;
class ProcessModelFactory;
class LayerFactory;
class ProcessModel;
class LayerFactory;
struct Inlets;
struct Outlets;

struct Inlets : ossia::small_vector<Process::Inlet*, 4>
{
  using small_vector::small_vector;
};
struct Outlets : ossia::small_vector<Process::Outlet*, 4>
{
  using small_vector::small_vector;
};
struct pan_weight : ossia::small_vector<double, 2>
{
  using small_vector::small_vector;
};
}
