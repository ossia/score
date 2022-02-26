#pragma once
#include <Crousti/Attributes.hpp>
#include <rnd/random.hpp>

namespace examples
{

struct SampleAccurateGeneratorExample
{
  meta_attribute(pretty_name, "My sample-accurate generator");
  meta_attribute(script_name, sample_acc_gen);
  meta_attribute(category, Demo);
  meta_attribute(author, "<AUTHOR>");
  meta_attribute(description, "<DESCRIPTION>");
  meta_attribute(uuid, "c519b3c4-326e-4e80-8dec-d465264c5b08");

  /**
   * Here we define a single output, which allows writing
   * sample-accurate data to an output port of the node.
   * Timestamps start from zero (at the beginning of a buffer) to N:
   * i ∈ [0; N( in the usual mathematic notation.
   */
  struct {
    struct {
      // Give a name to our parameter to show the user
      meta_attribute(name, "Out");

      // The data type used must conform to std::map<int64_t, your_type>.
      ossia::timed_vec<int> values;
    } value;
  } outputs;

  /**
   * Called at least once per cycle.
   * Note that buffer sizes aren't necessarily powers-of-two: N can be 1 for instance.
   *
   * The output buffer is guaranteed to be empty before the function is called.
   */
  void operator()(std::size_t N)
  {
    // 0 : first sample of the buffer.
    outputs.value.values[0] = rnd::rand(0, 100);
  }
};
}
