#include <Fx/FactorOracle2MIDI.hpp>

#define CATCH_CONFIG_MAIN 1
#if __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>

TEST_CASE("", "")
{

  using namespace ossia;
  using namespace Nodes::FactorOracle2MIDI;
  midi_port input, output;
  value_port regen;
  value_port bangs;
  int seq_len = 8;

  exec_state_facade facade;

  Node node;
  Node::State state;

  // Send the start of a note, wait 100 time units
  ossia::token_request req;
  {
    input.messages.push_back(libremidi::message::note_on(1, 64, 127));

    req.prev_date = 0_tv;
    req.date = 100_tv;
    node.run(input, regen, bangs, seq_len, output, req, {}, state);

    input.messages.clear();

    REQUIRE(output.messages.empty());
  }

  // wait 100 other time units
  {
    req.prev_date = 100_tv;
    req.date = 200_tv;
    node.run(input, regen, bangs, seq_len, output, req, {}, state);

    REQUIRE(output.messages.empty());
  }

  // Send the end of a note, wait 100 other time units
  {
    input.messages.push_back(libremidi::message::note_off(1, 64, 0));

    req.prev_date = 200_tv;
    req.date = 300_tv;
    node.run(input, regen, bangs, seq_len, output, req, {}, state);

    input.messages.clear();

    REQUIRE(output.messages.empty());
  }

  // Send a regen message
  {
    regen.write_value(bool(true), 0);

    req.prev_date = 300_tv;
    req.date = 400_tv;
    node.run(input, regen, bangs, seq_len, output, req, {}, state);

    regen.get_data().clear();

    REQUIRE(output.messages.empty());
  }

  // Send a bang message
  {
    bangs.write_value(ossia::impulse{}, 0);

    req.prev_date = 300_tv;
    req.date = 400_tv;
    node.run(input, regen, bangs, seq_len, output, req, {}, state);

    bangs.get_data().clear();

    REQUIRE(output.messages.size() == 1);
    REQUIRE(output.messages[0].bytes == libremidi::message::note_on(1, 64, 127).bytes);
  }
}
#endif
