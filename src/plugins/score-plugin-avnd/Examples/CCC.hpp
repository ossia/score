#pragma once
#include <Crousti/Attributes.hpp>
#include <Crousti/Widgets.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>
#include <avnd/wrappers/widgets.hpp>
namespace examples
{
/**
 * As an example, here is a port of the CCC chaotic engine part of
 * Litter Power, the traditional set of Max/MSP externals by Peter Castine.
 */
struct CCC
{
  meta_attribute(pretty_name, "CCC");
  meta_attribute(script_name, CCC);
  meta_attribute(category, Demo);
  meta_attribute(author, "Peter Castine");
  meta_attribute(description, "1/f noise, using the Schuster/Procaccia deterministic (chaotic) algorithm");
  meta_attribute(uuid, "9db0af3c-8573-4541-95d4-cf7902cdbedb");

  struct {
    /**
     * Here we use a bang input like the original Max external ;
     * notice that an ossia::impulse value as-is wouldn't make a lot of sense.
      **/
    avnd::sample_accurate::value_port<"Bang", avnd::impulse> bang;
  } inputs;

  struct {
    /** One float is output per bang **/
    avnd::sample_accurate::value_port<"Out", float> out;
  } outputs;

  /** We need to keep some state around **/
  double current_value{0.1234};

  /** No particular argument is needed here, we can just process the whole input buffer **/
  void operator()()
  {
    for(auto& [timestamp, value] : inputs.bang.values)
    {
      // CCC algorithm, copied verbatim from the LitterPower source code.
      {
        constexpr double kMinPink = 1.0 / 525288.0;

        double curVal = this->current_value;

        // Sanity check... due to limitations in accuracy, we can die at very small values.
        // Also, we prefer to only "nudge" the value towards chaos...
        if (curVal <= kMinPink) {
          if (curVal == 0.0)	curVal  = kMinPink;
          else				curVal += curVal;
        }

        curVal = curVal * curVal + curVal;
        if (curVal >= 1.0)					// Cheaper than fmod(), and works quite nicely
          curVal -= 1.0;					// in the range of values that can occur.

        this->current_value = curVal;
      }

      outputs.out.values[timestamp] = current_value;
    }
  }
};
}
