#pragma once
#include <Crousti/Attributes.hpp>

namespace examples
{

struct TrivialGeneratorExample
{
  meta_attribute(pretty_name, "My trivial generator");
  meta_attribute(script_name, trivial_effect_gen);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "29099d09-cbd1-451b-8394-972b0d5bfaf0");

  /**
   * Here we define a single output, which allows writing
   * a single output value every time the process is run.
   */
  struct {
    struct {
      // Give a name to our parameter to show the user
      meta_attribute(name, "Out");

      // This value will be sent to the output of the port at each tick.
      // The name "value" is important.
      int value{};
    } main; // This variable can be called however you wish.
  } outputs; // This must be called "outputs".

  /**
   * Called at least once per cycle.
   * The value will be stored at the start of the tick.
   */
  void operator()()
  {
    outputs.main.value++;
    if(outputs.main.value > 100)
      outputs.main.value = 0;
  }
};

}
