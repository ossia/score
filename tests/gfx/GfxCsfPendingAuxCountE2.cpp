// Pins the expression log of a CSF whose auxiliary SIZE reads $COUNT_<name> of
// a read_only auxiliary its upstream geometry provides (the shape of
// cull_lights_into_clusters.csf and $COUNT_cluster_aabbs). Before the upstream
// buffer is bound the name is declared but has no size yet: resolving it logs
// nothing and the dispatch still sizes against the real upstream count. A name
// nothing declares still logs ERR232.
#include <score_test/Gfx.hpp>

#include <ossia/detail/logger.hpp>

#include <spdlog/sinks/base_sink.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <mutex>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
struct CaptureSink final : spdlog::sinks::base_sink<std::mutex>
{
  std::vector<std::string> lines;

protected:
  void sink_it_(const spdlog::details::log_msg& msg) override
  {
    lines.emplace_back(msg.payload.data(), msg.payload.size());
  }
  void flush_() override { }
};

QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct Run
{
  bool skipped{};
  std::string err;
  std::array<uint8_t, 4> px{};
  std::vector<std::string> log;
};

Run run(score::gfx::GraphicsApi api, const char* consumer)
{
  Run r;
  auto sink = std::make_shared<CaptureSink>();
  auto& sinks = ossia::logger().sinks();
  sinks.push_back(sink);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addCsf(corpus("csf-a5-aux-producer.cs"));
    const int cons = p.addCsf(corpus(consumer));
    if(producer < 0 || cons < 0)
    {
      r.err = "node build failed: " + p.error();
      return;
    }
    p.wire(p.geometryOut(producer, 0), p.geometryIn(cons, 0));
    const int s = p.addSink({32, 32});
    p.wire(p.imageOut(cons, 0), p.sinkInput(s));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    auto img = p.readback(s);
    if(!img.valid())
    {
      r.err = "readback failed";
      return;
    }
    r.px = img.center();
  });
  std::erase(sinks, sink);
  r.log = sink->lines;
  return r;
}

int count(const std::vector<std::string>& log, const std::string& needle)
{
  int n = 0;
  for(const auto& l : log)
    if(l.find(needle) != std::string::npos)
      ++n;
  return n;
}
}

TEST_CASE(
    "CSF SIZE on a pending upstream auxiliary count resolves without ERR232",
    "[gfx][csf][expression][aux]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run r = run(api, "csf-a5-aux-consumer.cs");
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());
  for(const auto& l : r.log)
    UNSCOPED_INFO(l);
  CHECK(count(r.log, "ERR232") == 0);
  CHECK(int(r.px[0]) == 24);
  CHECK(int(r.px[1]) == 24);
}

TEST_CASE(
    "CSF SIZE on an undeclared count still logs ERR232",
    "[gfx][csf][expression][aux]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run r = run(api, "e2-csf-count-undeclared.cs");
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());
  CHECK(count(r.log, "var_COUNT_itemz") > 0);
  CHECK(count(r.log, "var_COUNT_items'") == 0);
}
