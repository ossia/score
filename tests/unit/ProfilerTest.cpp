#include <Execution/Profiler/Profiler.hpp>

#include <QJsonArray>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace Execution;

TEST_CASE("ProfilerSeries keeps the last values", "[profiler]")
{
  ProfilerSeries s;
  REQUIRE(s.empty());
  REQUIRE(s.avg() < 0.);
  for(int i = 1; i <= ProfilerSeries::size + 10; i++)
    s.push(i);
  REQUIRE(s.last() == ProfilerSeries::size + 10);
  REQUIRE(s.max() == ProfilerSeries::size + 10);
  REQUIRE(s.avg() == Approx((11 + ProfilerSeries::size + 10) / 2.));
  s.clear();
  REQUIRE(s.empty());
}

TEST_CASE("Render frames feed the nodes and the frame totals", "[profiler]")
{
  Profiler p{nullptr};
  RenderFrameReport f;
  f.cpuNs = 2'000'000;
  f.gpuNs = 5'000'000;
  RenderFrameReport::Node a;
  a.key = 3;
  a.name = "Output";
  a.cpuNs = 100'000;
  a.gpuNs = 1'000'000;
  a.targetBytes = 1024;
  RenderFrameReport::Node b;
  b.key = 5;
  b.name = "Filter";
  b.cpuNs = 300'000;
  b.gpuNs = -1;
  f.nodes = {a, b};
  std::vector<RenderFrameReport> frames{f, f};
  frames[1].nodes[0].gpuNs = 3'000'000;
  p.pushRender(frames);

  const auto& e = p.entries();
  REQUIRE(e.size() == 2);
  const auto& out = e.at(3);
  REQUIRE(out.name == "Output");
  REQUIRE(!out.isProcess);
  REQUIRE(out.gpuNs.avg() == Approx(2'000'000));
  REQUIRE(out.gpuNs.max() == Approx(3'000'000));
  REQUIRE(out.targetBytes == 1024);
  REQUIRE(e.at(5).gpuNs.empty());
  REQUIRE(e.at(5).renderCpuNs.avg() == Approx(300'000));
  REQUIRE(p.frameGpuNs().avg() == Approx(5'000'000));
  REQUIRE(p.gpuHistory().size() == 2);
  REQUIRE(p.spikes().empty());
}

TEST_CASE("Frames over the threshold are kept with their slowest nodes", "[profiler]")
{
  Profiler p{nullptr};
  p.setRenderSpikeMs(4.);
  std::vector<RenderFrameReport> frames(3);
  for(int i = 0; i < 3; i++)
  {
    frames[i].frame = 10 + i;
    frames[i].cpuNs = 1'000'000;
    frames[i].gpuNs = i == 1 ? 9'000'000 : 2'000'000;
    for(int n = 0; n < 10; n++)
    {
      RenderFrameReport::Node node;
      node.key = n + 1;
      node.name = QString::number(n);
      node.gpuNs = n * 100'000;
      frames[i].nodes.push_back(node);
    }
  }
  p.pushRender(frames);
  REQUIRE(p.spikes().size() == 1);
  const auto& s = p.spikes().front();
  REQUIRE(s.kind == ProfilerSpike::Render);
  REQUIRE(s.frame == 11);
  REQUIRE(s.otherMs == Approx(9.));
  REQUIRE(s.top.size() == 8);
  REQUIRE(s.top.front().first == "9");
}

TEST_CASE("Audio load is over the report's whole duration", "[profiler]")
{
  Profiler p{nullptr};
  AudioBenchReport r;
  r.ticks = 50;
  r.tickSum = 50 * 1'000'000;
  r.tickMax = 3'000'000;
  r.budgetNs = 50 * 5'000'000.;
  r.spikes[0].totalNs = 4'500'000;
  r.spikes[0].budgetNs = 5'000'000.;
  r.spikeCount = 1;
  p.pushAudio(r, [](const ossia::graph_node*) { return nullptr; });
  REQUIRE(p.audioLoad().last() == Approx(20.));
  REQUIRE(p.audioTickNs().last() == Approx(1'000'000));
  REQUIRE(p.spikes().size() == 1);
  REQUIRE(p.spikes().front().kind == ProfilerSpike::Audio);
  REQUIRE(p.spikes().front().otherMs == Approx(5.));

  const auto json = p.toJson();
  REQUIRE(json["audio"].toObject()["loadPercent"].toObject()["last"].toDouble()
          == Approx(20.));
  REQUIRE(json["spikes"].toArray().size() == 1);

  p.clear();
  REQUIRE(p.entries().empty());
  REQUIRE(p.spikes().empty());
  REQUIRE(p.audioLoad().empty());
}

TEST_CASE("Each kind of spike has its own cap", "[profiler]")
{
  Profiler p{nullptr};
  p.setRenderSpikeMs(1.);
  AudioBenchReport r;
  r.ticks = 1;
  r.tickSum = 1;
  r.budgetNs = 1.;
  r.spikeCount = 5;
  p.pushAudio(r, [](const ossia::graph_node*) { return nullptr; });
  std::vector<RenderFrameReport> frames(500);
  for(auto& f : frames)
    f.gpuNs = 2'000'000;
  p.pushRender(frames);
  int audio = 0;
  for(const auto& s : p.spikes())
    audio += s.kind == ProfilerSpike::Audio;
  REQUIRE(audio == 5);
  REQUIRE(p.spikes().size() == 205);
}

TEST_CASE("The spike generation moves on past the cap", "[profiler]")
{
  Profiler p{nullptr};
  p.setRenderSpikeMs(1.);
  std::vector<RenderFrameReport> frames(300);
  for(auto& f : frames)
    f.gpuNs = 2'000'000;
  p.pushRender(frames);
  const auto full = p.spikes().size();
  const auto generation = p.spikeGeneration();
  p.pushRender(std::span{frames}.first(1));
  CHECK(p.spikes().size() == full);
  CHECK(p.spikeGeneration() != generation);
  p.clear();
  CHECK(p.spikeGeneration() != generation + 1);
}

TEST_CASE("Audio reports go round between the audio thread and the GUI", "[profiler]")
{
  AudioBenchChannel channel;
  std::vector<AudioBenchReport*> published;
  for(int i = 0; i < 3; i++)
  {
    auto* r = channel.current();
    REQUIRE(r);
    r->ticks = i + 1;
    published.push_back(r);
    channel.publish();
  }
  CHECK(channel.current() == nullptr);

  std::vector<int> ticks;
  channel.drain([&](const AudioBenchReport& r) { ticks.push_back(r.ticks); });
  CHECK(ticks == std::vector<int>{1, 2, 3});

  auto* again = channel.current();
  REQUIRE(again);
  CHECK(again->ticks == 0);
  CHECK(again->spikeCount == 0);
  CHECK(again->nodes.empty());
}

