// SPDX-License-Identifier: GPL-3.0-or-later
#include <Fx/VelToNote.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>

#include <avnd/binding/ossia/data_node.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace
{
using NoteNode = Nodes::PulseToNote::Node;
using Packet = std::pair<std::int64_t, std::uint32_t>;

struct MidiConsumer final : ossia::nonowning_graph_node
{
  ossia::midi_inlet input;
  std::vector<Packet> received;
  MidiConsumer() { m_inlets.push_back(&input); }
  std::string label() const noexcept override { return "VelToNote consumer"; }
  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override
  {
    for(const auto& packet : input.data.messages)
    {
      const auto message = libremidi::midi1_from_ump(packet);
      if(message.bytes.size() == 3)
        received.emplace_back(
            packet.timestamp, (std::uint32_t(message.bytes[0]) << 16)
                                  | (std::uint32_t(message.bytes[1]) << 8)
                                  | message.bytes[2]);
    }
  }
};

struct MidiGraph
{
  ossia::execution_state state;
  std::shared_ptr<oscr::safe_node<NoteNode>> source;
  std::shared_ptr<MidiConsumer> consumer = std::make_shared<MidiConsumer>();
  std::shared_ptr<ossia::graph_interface> graph = ossia::make_graph({});

  explicit MidiGraph(int frames = 16)
  {
    state.sampleRate = 1000;
    state.bufferSize = frames;
    state.samples_since_start = frames;
    state.modelToSamplesRatio = 1000. / 705600000.;
    state.samplesToModelRatio = 705600000. / 1000.;
    source = std::make_shared<oscr::safe_node<NoteNode>>(frames, 1000., 17);
    source->finish_init();
    auto& effect = source->impl.effect;
    effect.ossia_state = {&state};
    effect.inputs.start_quant.value = 0.;
    effect.inputs.end_quant.value = 0.;
    effect.inputs.basenote.value = 60;
    effect.inputs.basevel.value = 100;
    graph->add_node(source);
    graph->add_node(consumer);
    graph->connect(graph->allocate_edge(
        ossia::immediate_glutton_connection{}, source->root_outputs()[0],
        &consumer->input, source, consumer));
  }

  static ossia::token_request
  token(int offset, int frames, double tempo = 120., std::int64_t first_model = 0)
  {
    ossia::token_request tk;
    tk.start_sample = offset;
    tk.length_sample = frames;
    tk.tempo = tempo;
    tk.prev_date.impl = first_model;
    tk.date.impl = first_model + std::int64_t(frames * 5880 * tempo);
    tk.musical_start_position = 2. * tk.prev_date.impl / 705600000.;
    tk.musical_end_position = 2. * tk.date.impl / 705600000.;
    return tk;
  }

  void input(int pitch, int date)
  {
    source->impl.effect.inputs.port.value->write_value(pitch, date);
  }

  void publish()
  {
    consumer->request(token(0, state.bufferSize));
    graph->state(state);
    state.commit();
  }
};
}

TEST_CASE(
    "VelToNote publishes all slices once through a real graph cable",
    "[nodes][veltonote][sdk]")
{
  MidiGraph f;
  f.input(60, 7);
  f.source->request(MidiGraph::token(4, 4));
  f.source->request(MidiGraph::token(8, 4, 120., 2822400));
  f.source->request(MidiGraph::token(12, 4, 120., 5644800));
  f.publish();
  REQUIRE(f.consumer->received == std::vector<Packet>{{7, 0x903c64}, {8, 0x803c00}});

  // A new publication with a repeated counter and position cannot replay MIDI.
  f.consumer->received.clear();
  f.source->request(MidiGraph::token(4, 4));
  f.publish();
  REQUIRE(f.consumer->received.empty());
}

TEST_CASE(
    "VelToNote synchronized chooser is converted once by the real binding",
    "[nodes][veltonote][sdk]")
{
  for(int tempo : {60, 120, 240})
  {
    INFO("tempo=" << tempo);
    const int off = 60000 / tempo;
    MidiGraph f{off + 2};
    auto& effect = f.source->impl.effect;
    effect.inputs.fixed_duration.value = true;
    effect.inputs.duration.sync = true;
    // Native chooser controls store whole-note fractions, not seconds.
    using Inputs = decltype(effect.inputs);
    constexpr auto duration_index = avnd::index_in_struct(Inputs{}, &Inputs::duration);
    f.source->time_controls.update_control(
        f.source->impl, avnd::field_index<duration_index>{}, .25f, true);
    f.input(60, 0);
    f.source->request(MidiGraph::token(0, off + 2, tempo));
    f.publish();
    REQUIRE(f.consumer->received == std::vector<Packet>{{0, 0x903c64}, {off, 0x803c00}});
  }
}

TEST_CASE(
    "VelToNote cleanup reaches its cable when a writable token exists",
    "[nodes][veltonote][sdk]")
{
  MidiGraph f;
  f.source->impl.effect.inputs.end_quant.value = -1.;
  f.input(60, 0);
  f.source->request(MidiGraph::token(0, 16));
  f.publish();
  REQUIRE(f.consumer->received == std::vector<Packet>{{0, 0x903c64}});
  f.consumer->received.clear();
  f.source->impl.effect.stop();
  REQUIRE(f.source->impl.effect.needs_service());
  // This explicitly supplies the window absent from score's hard-stop path.
  f.state.samples_since_start += 16;
  f.source->request(MidiGraph::token(0, 16, 120., 11289600));
  f.publish();
  REQUIRE(f.consumer->received == std::vector<Packet>{{0, 0x803c00}});
  REQUIRE_FALSE(f.source->impl.effect.needs_service());
}
