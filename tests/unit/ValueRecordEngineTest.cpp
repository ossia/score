// The value recorder of the recording engine, driven tick by tick (see
// AudioRecordEngineTest for the clock), and the value take it makes:
//  - every value is dated in the box where it arrived;
//  - one bar long, the take ends on the bar, whatever the tempo;
//  - the take file is the loopers' "score-value-take" format: frames since
//    the start, typed values, and for numbers their count and range, which is
//    what an automation plays; values an automation cannot play say so
//    (components 0).

#include <Recording/Engine/ValueRecorder.hpp>
#include <Recording/Takes/RecordedValues.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/dataflow/value_port.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace Recording;

namespace
{
constexpr int rate = 48000;
constexpr int buffer = 480;
constexpr int64_t flicks_per_second = 705600000;
constexpr int64_t tick_flicks = flicks_per_second / 100;
constexpr int64_t flicks_per_sample = flicks_per_second / rate;
constexpr double Q = 352800000.0;

struct Rig
{
  ossia::execution_state st;
  std::shared_ptr<ossia::nodes::forward_node> clock
      = std::make_shared<ossia::nodes::forward_node>();
  std::shared_ptr<ValueTake> take = std::make_shared<ValueTake>(4096);
  value_record_node node{take, clock};
  int64_t boxDate = 0;
  std::vector<ValueTake::Event> events;

  Rig()
  {
    st.sampleRate = rate;
    st.bufferSize = buffer;
    st.modelToSamplesRatio = double(rate) / flicks_per_second;
    st.samplesToModelRatio = flicks_per_second / double(rate);
  }

  void tick(std::vector<std::pair<int, ossia::value>> vals = {})
  {
    tickInParts(std::move(vals), 1);
  }

  //! The tick as two tokens one after the other, each half of the buffer
  void tickInTwo(std::vector<std::pair<int, ossia::value>> vals)
  {
    tickInParts(std::move(vals), 2);
  }

  void tickInParts(std::vector<std::pair<int, ossia::value>> vals, int parts)
  {
    auto& in = *node.root_inputs()[0]->target<ossia::value_port>();
    in.get_data().clear();
    for(auto& [at, v] : vals)
      in.get_data().emplace_back(v, at);

    clock->requested_tokens.clear();
    for(int p = 0; p < parts; p++)
    {
      const int64_t from = boxDate + tick_flicks * p / parts;
      const int64_t to = boxDate + tick_flicks * (p + 1) / parts;
      ossia::token_request tk;
      tk.prev_date = ossia::time_value{from};
      tk.date = ossia::time_value{to};
      tk.offset = ossia::time_value{from - boxDate};
      tk.speed = 1.;
      tk.tempo = 120.;
      tk.signature = ossia::time_signature{4, 4};
      tk.start_sample = buffer * p / parts;
      tk.length_sample = buffer / parts;
      tk.musical_start_position = from / Q;
      tk.musical_end_position = to / Q;
      tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
      tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;
      clock->requested_tokens.push_back(tk);
    }

    ossia::graph_node& base = node;
    base.run(ossia::token_request{}, ossia::exec_state_facade{&st});
    boxDate += tick_flicks;

    ValueTake::Event ev;
    while(take->queue.try_dequeue(ev))
      events.push_back(ev);
  }
};
}

TEST_CASE("In a tick cut in two, each value is recorded once, where it arrived", "[recording][values]")
{
  // A box that loops, or whose tempo changes, gives two tokens in one tick
  Rig r;
  r.take->startRequested = true;
  r.tick();
  const int64_t tick_start = r.boxDate;
  r.tickInTwo({{10, 1.f}, {239, 2.f}, {240, 3.f}, {470, 4.f}});
  r.take->stopRequested = true;
  r.tick();
  REQUIRE(r.events.size() == 4);
  for(int i = 0; i < 4; i++)
    CHECK(*r.events[i].value.target<float>() == float(i + 1));
  CHECK(r.events[0].date == tick_start + 10 * flicks_per_sample);
  CHECK(r.events[1].date == tick_start + 239 * flicks_per_sample);
  CHECK(r.events[2].date == tick_start + 240 * flicks_per_sample);
  CHECK(r.events[3].date == tick_start + 470 * flicks_per_sample);
}

TEST_CASE("From the first value, a take starts in the part of the tick the value is in", "[recording][values]")
{
  Rig r;
  r.take->startOnFirstEvent = true;
  r.take->startRequested = true;
  r.tick();
  const int64_t tick_start = r.boxDate;
  r.tickInTwo({{300, 1.f}});
  r.take->stopRequested = true;
  r.tick();
  REQUIRE(r.events.size() == 1);
  CHECK(r.events[0].date == tick_start + 300 * flicks_per_sample);
  CHECK(r.take->startDate == tick_start + 300 * flicks_per_sample);
}

TEST_CASE("A value is dated in the box where it arrived", "[recording][values]")
{
  Rig r;
  r.take->startRequested = true;
  r.tick({{10, 0.5f}});
  r.tick({{200, ossia::vec2f{1.f, 2.f}}});
  r.take->stopRequested = true;
  r.tick();
  REQUIRE(r.events.size() == 2);
  CHECK(r.events[0].date == 10 * flicks_per_sample);
  CHECK(r.events[1].date == tick_flicks + 200 * flicks_per_sample);
  CHECK(r.take->state == ValueTake::Done);
}

TEST_CASE("One bar long, a value take ends on the bar", "[recording][values]")
{
  Rig r;
  r.take->lengthBars = 1.;
  r.take->startRequested = true;
  for(int i = 0; i < 199; i++)
    r.tick({{0, float(i)}});
  CHECK(r.take->state == ValueTake::Recording);
  r.tick({{0, 199.f}});
  // The bar ends at 2 s, the end of tick 200
  CHECK(r.take->state == ValueTake::Done);
  CHECK(r.take->endDate == 200 * tick_flicks);
  r.tick({{0, 200.f}});
  CHECK(r.events.size() == 200);
}

TEST_CASE("A value take is the loopers' value take format", "[recording][values]")
{
  std::vector<ValueTake::Event> ev{
      {1000 * flicks_per_sample, ossia::value{0.25f}},
      {2000 * flicks_per_sample, ossia::value{0.75f}},
      {3000 * flicks_per_sample, ossia::value{0.5f}}};
  const auto json = valueTakeJson(ev, 1000 * flicks_per_sample, 4000 * flicks_per_sample,
                                  rate, 120., 3, 4);
  const auto o = QJsonDocument::fromJson(json).object();
  CHECK(o["format"].toString() == "score-value-take");
  CHECK(o["rate"].toInt() == rate);
  CHECK(o["frames"].toDouble() == 3000.);
  CHECK(o["components"].toInt() == 1);
  CHECK(o["min"].toArray()[0].toDouble() == 0.25);
  CHECK(o["max"].toArray()[0].toDouble() == 0.75);
  CHECK(o["meter"].toArray() == QJsonArray{3, 4});
  const auto evs = o["events"].toArray();
  REQUIRE(evs.size() == 3);
  CHECK(evs[0].toArray()[0].toDouble() == 0.);
  CHECK(evs[0].toArray()[1].toString() == "float");
  CHECK(evs[2].toArray()[0].toDouble() == 2000.);

  // Vectors: as many components as numbers
  std::vector<ValueTake::Event> v3{{0, ossia::value{ossia::vec3f{1, 2, 3}}}};
  CHECK(numericComponents(v3) == 3);

  // A string: an automation cannot play it
  std::vector<ValueTake::Event> s{{0, ossia::value{1.f}}, {10, ossia::value{std::string{"a"}}}};
  CHECK(numericComponents(s) == 0);
  const auto so = QJsonDocument::fromJson(valueTakeJson(s, 0, 100, rate, 120., 4, 4)).object();
  CHECK(so["components"].toInt() == 0);
  CHECK(so["events"].toArray()[1].toArray()[1].toString() == "string");
}
