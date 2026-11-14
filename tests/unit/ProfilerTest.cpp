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


namespace
{
RenderFrameReport frameAt(const void* output, int64_t timeNs)
{
  RenderFrameReport f;
  f.output = output;
  f.timeNs = timeNs;
  f.cpuNs = 1'000'000;
  return f;
}
}

TEST_CASE("The frame rate is the fastest output's", "[profiler]")
{
  Profiler p{nullptr};
  REQUIRE(p.framesPerSecond(0) == 0.);
  const int a = 0, b = 0;
  const int64_t t0 = 10'000'000'000;
  std::vector<RenderFrameReport> frames;
  for(int i = 0; i <= 30; i++)
  {
    frames.push_back(frameAt(&a, t0 + int64_t(i) * 1'000'000'000 / 60));
    if(i % 2 == 0)
      frames.push_back(frameAt(&b, t0 + int64_t(i) * 1'000'000'000 / 60));
  }
  p.pushRender(frames);
  const int64_t now = t0 + int64_t(30) * 1'000'000'000 / 60;
  CHECK(p.framesPerSecond(now) == Approx(60.).epsilon(1e-3));
  CHECK(p.frameCpuNs().avg() == Approx(1'000'000));
}

TEST_CASE("An output that stopped rendering no longer sets the frame rate", "[profiler]")
{
  Profiler p{nullptr};
  const int fast = 0, slow = 0;
  const int64_t t0 = 10'000'000'000;
  std::vector<RenderFrameReport> frames;
  for(int i = 0; i < 10; i++)
    frames.push_back(frameAt(&fast, t0 + int64_t(i) * 10'000'000));
  p.pushRender(frames);
  CHECK(p.framesPerSecond(t0 + 90'000'000) == Approx(100.).epsilon(1e-3));

  frames.clear();
  const int64_t t1 = t0 + 2 * Profiler::frameTimeoutNs;
  for(int i = 0; i < 10; i++)
    frames.push_back(frameAt(&slow, t1 + int64_t(i) * 50'000'000));
  p.pushRender(frames);
  const int64_t now = t1 + 9 * 50'000'000;
  CHECK(p.framesPerSecond(now) == Approx(20.).epsilon(1e-3));
  CHECK(p.framesPerSecond(now + 2 * Profiler::frameTimeoutNs) == 0.);

  frames = {frameAt(&fast, now), frameAt(&fast, now + 1'000'000)};
  p.pushRender(frames);
  CHECK(p.framesPerSecond(now + 1'000'000) == Approx(1000.).epsilon(1e-3));
}

TEST_CASE("An output stage report adds its nodes and no frame", "[profiler]")
{
  Profiler p{nullptr};
  const int output = 0;
  RenderFrameReport stage;
  stage.output = &output;
  stage.nodesOnly = true;
  stage.timeNs = 10'000'000'000;
  stage.cpuNs = 123;
  stage.gpuNs = 456;
  RenderFrameReport::Node n;
  n.key = (quintptr(7) << 2) | 3;
  n.name = "Output stage";
  n.cpuNs = 200'000;
  n.gpuNs = 1'500'000;
  stage.nodes = {n};
  std::vector<RenderFrameReport> frames{stage, stage};
  frames[1].timeNs += 16'000'000;
  p.pushRender(frames);

  const auto& e = p.entries();
  REQUIRE(e.size() == 1);
  const auto& out = e.at(n.key);
  CHECK(out.name == "Output stage");
  CHECK_FALSE(out.isProcess);
  CHECK(out.renderCpuNs.avg() == Approx(200'000));
  CHECK(out.gpuNs.avg() == Approx(1'500'000));
  CHECK(p.frameCpuNs().empty());
  CHECK(p.frameGpuNs().empty());
  CHECK(p.renderCpuHistory().empty());
  CHECK(p.framesPerSecond(frames[1].timeNs) == 0.);
}
