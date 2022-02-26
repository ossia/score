#pragma once
#include <Crousti/Attributes.hpp>
#include <cmath>

namespace examples
{

struct TrivialFilterExample
{
  meta_attribute(pretty_name, "My trivial filter");
  meta_attribute(script_name, trivial_effect_filter);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "d02006f0-3e71-465b-989c-7c53aaa885e5");

  /**
   * Here we define a pair of input / outputs.
   */
  struct {
    struct {
      // Give a name to our parameter to show the user
      meta_attribute(name, "In");

      float value{};
    } main; // Name does not matter
  } inputs; // Inputs have to be inside an "input" member.

  struct {
    struct {
      meta_attribute(name, "Out");

      int value{};
    } main;
  } outputs;

  /**
   * Called at least once per cycle.
   * The value will be stored at the start of the tick.
   */
  void operator()()
  {
    outputs.main.value = inputs.main.value > 0 ? std::log(inputs.main.value) : 0;
  }
};

}
