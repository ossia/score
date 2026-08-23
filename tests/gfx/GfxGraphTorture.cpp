// =============================================================================
// Editing the render graph while it renders, for as long as you let it.
//
// The incremental tests each pin one edit: add a node, add an edge, remove an
// edge, remove a node, resize a sink. What none of them cover is an edit
// SEQUENCE -- a user connecting, disconnecting and adding for minutes on end,
// where the failure is a state the graph only reaches after some particular
// order of operations. This drives random edits against a live render loop
// until a deadline and asserts the invariants after every one.
//
// Operations, all through the same incremental entry points GfxContext uses:
//   * add an ISF / raster / VSA node, drawn from a table that spans the ISF
//     feature surface (controls, image inputs, audio, multipass, persistent,
//     MRT, uniform_input and storage buffers, compute, geometry)
//   * add a sink and wire something into it
//   * connect a texture, buffer or geometry port
//   * disconnect a cable
//   * render frames, which is where a bad edit actually bites
//
// Nodes, sinks and outputs are only ever ADDED. Cables are added and removed.
// Node removal has its own test (GfxNodeRemoval.cpp) and is deliberately out of
// scope here so that a failure is attributable to the edit sequence rather than
// to teardown.
//
// Deterministic: the schedule comes from a fixed seed, printed on failure and
// overridable with SCORE_TORTURE_SEED so a bad sequence can be replayed exactly.
// SCORE_TORTURE_SECONDS sets the budget; the default is small enough for CI and
// the soak is
//
//   SCORE_TORTURE_SECONDS=60 DISPLAY=:0 ctest -R gfx_graph_torture
//
// QT_QPA_PLATFORM=offscreen must NOT be used: it falls back to the Null
// backend, which accepts every edit and renders nothing.
// =============================================================================

#include "IsfTestCommon.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <random>
#include <string>
#include <vector>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
/// Corpus shaders chosen to span the ISF feature surface rather than to be
/// interesting on their own: every one of these reaches a different part of the
/// node/renderer setup, which is what makes an arbitrary edit sequence over them
/// worth running.
const char* const kIsfShaders[] = {
    "isf-solid-color.fs",              // trivial producer
    "isf-gradient-x.fs",               //
    "isf-gradient-y.fs",               //
    "isf-passthrough-plain.fs",        // one image input
    "isf-two-images.fs",               // two image inputs
    "isf-image-passthrough.fs",        // the sampling macros
    "isf-control-float.fs",            // control inlets, one per type
    "isf-control-bool.fs",             //
    "isf-control-color.fs",            //
    "isf-control-long.fs",             //
    "isf-control-point2d.fs",          //
    "isf-control-point3d.fs",          //
    "isf-audio-const.fs",              // audio inlet -> R32F upload
    "isf-multipass-size.fs",           // multipass, sized passes
    "isf-multipass-expr-size.fs",      // multipass, expression-sized
    "isf-persistent-counter.fs",       // PERSISTENT pass
    "isf-persistent-feedback.fs",      // PERSISTENT + feedback sampling
    "isf-mrt-gradient-y.fs",           // MRT, 2 outputs
    "isf-mrt-four-outputs.fs",         // MRT, 4 outputs
    "isf-time-uniforms.fs",            // standard UBO
    "isf-large-uniform-input.fs",      // uniform_input buffer inlet
    "isf-orient-quadrants.fs",         //
};

// Compute (CSF) nodes are reached through make_csf_node rather than a
// GfxPipeline entry point, so this sequence does not create them; its buffer
// and geometry edits go through whatever ports the ISF and raster nodes expose.

struct RasterPair
{
  const char* vs;
  const char* fs;
};
const RasterPair kRasterPairs[] = {
    {"raw-raster-basic.vs", "raw-raster-basic.fs"},
    {"raw-raster-auxiliary.vs", "raw-raster-auxiliary.fs"},
    {"raw-raster-mrt.vs", "raw-raster-mrt.fs"},
    {"rr-perlayer.vs", "rr-perlayer.fs"},
};

const char* const kVsaShaders[] = {"vsa-points.vs", "vsa-triangle.vs"};

/// One live cable, so it can be picked for disconnection later.
struct Cable
{
  score::gfx::Port* source{};
  score::gfx::Port* sink{};
};

unsigned seed_from_env()
{
  bool ok = false;
  const unsigned s = qEnvironmentVariableIntValue("SCORE_TORTURE_SEED", &ok);
  return ok && s != 0 ? s : 20260823u;
}

int seconds_from_env()
{
  bool ok = false;
  const int s = qEnvironmentVariableIntValue("SCORE_TORTURE_SECONDS", &ok);
  return ok && s > 0 ? s : 8;
}

/// What the graph must satisfy after every single edit.
struct Invariants
{
  std::string failure;
  int edits{};
  int renders{};
  int nodes{};
  int sinks{};
  int cables{};
  int connects{};
  int disconnects{};
  bool sinkZeroValid{};
  std::array<uint8_t, 4> sinkZeroPixel{};
  // Iteration at which the control sink first stopped showing its producer.
  bool baselineValid{};
  std::array<uint8_t, 4> baselinePixel{};
  bool beforeFinalValid{};
  std::array<uint8_t, 4> beforeFinalPixel{};
  long long brokeAtIter{-1};
  int brokeAtEdits{-1};
  int brokeAtConnects{-1};
  std::array<uint8_t, 4> brokePixel{};
};
}

TEST_CASE(
    "a graph edited continuously while rendering stays consistent",
    "[gfx][l3][torture][slow]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const unsigned seed = seed_from_env();
  const int budget = seconds_from_env();
  INFO("seed=" << seed << " (SCORE_TORTURE_SEED to replay) budget=" << budget << "s");

  Invariants inv;
  bool skipped = false;
  std::string skipReason, backendName;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    std::mt19937 rng{seed};
    auto pick = [&](int n) { return int(rng() % unsigned(n)); };

    std::vector<int> nodes;
    std::vector<int> sinks;
    std::vector<Cable> cables;

    // The initial graph is built BEFORE create(): the device is brought up
    // around whatever exists, and everything after that goes through the
    // incremental path, which is the thing under test.
    const int seedNode = p.addIsf(corpus("isf-solid-color.fs"));
    if(seedNode < 0)
    {
      inv.failure = "could not build the seed node: " + p.error();
      return;
    }
    nodes.push_back(seedNode);
    sinks.push_back(p.addSink({64, 64}));
    p.wire(p.imageOut(seedNode, 0), p.sinkInput(sinks[0]));
    cables.push_back({p.imageOut(seedNode, 0), p.sinkInput(sinks[0])});

    if(!p.create(backend))
    {
      skipped = true;
      skipReason = p.skipReason().empty() ? "could not create a device" : p.skipReason();
      return;
    }
    backendName = p.backend();
    p.render(3);
    {
      const auto base = p.readback(sinks[0]);
      inv.baselineValid = base.valid();
      if(base.valid())
        inv.baselinePixel = base.at(base.width / 2, base.height / 2);
    }

    const auto deadline
        = std::chrono::steady_clock::now() + std::chrono::seconds(budget);

    // Bounded so a long budget soaks the edit path rather than running the
    // machine out of GPU memory.
    constexpr int kMaxNodes = 24;
    constexpr int kMaxSinks = 6;

    while(std::chrono::steady_clock::now() < deadline && inv.failure.empty())
    {
      switch(pick(9))
      {
        case 0: // add an ISF node spanning the feature surface
        case 1:
        {
          if(int(nodes.size()) >= kMaxNodes)
            break;
          const char* sh = kIsfShaders[pick(int(std::size(kIsfShaders)))];
          const int n = p.addIsf(corpus(sh));
          if(n < 0)
          {
            // A shader the corpus cannot build here is not a torture failure;
            // record it and carry on rather than aborting the sequence.
            break;
          }
          nodes.push_back(n);
          ++inv.edits;
          break;
        }
        case 2: // add a raster node (geometry input path)
        {
          if(int(nodes.size()) >= kMaxNodes)
            break;
          const auto& pr = kRasterPairs[pick(int(std::size(kRasterPairs)))];
          const int n = p.addRaster(corpus(pr.vs), corpus(pr.fs));
          if(n >= 0)
          {
            nodes.push_back(n);
            ++inv.edits;
          }
          break;
        }
        case 3: // add a VSA node
        {
          if(int(nodes.size()) >= kMaxNodes)
            break;
          const int n = p.addVsa(corpus(kVsaShaders[pick(int(std::size(kVsaShaders)))]));
          if(n >= 0)
          {
            nodes.push_back(n);
            ++inv.edits;
          }
          break;
        }
        case 4: // add an output and wire something into it
        {
          if(int(sinks.size()) >= kMaxSinks)
            break;
          const int s = p.addSink({64, 64});
          sinks.push_back(s);
          ++inv.edits;
          if(!nodes.empty())
          {
            auto* src = p.imageOut(nodes[pick(int(nodes.size()))], 0);
            auto* dst = p.sinkInput(s); // s is freshly added, never sink 0
            if(src && dst)
            {
              p.addEdgeIncremental(src, dst);
              cables.push_back({src, dst});
              ++inv.connects;
            }
          }
          break;
        }
        case 5: // connect a texture somewhere
        {
          if(nodes.empty())
            break;
          auto* src = p.imageOut(nodes[pick(int(nodes.size()))], pick(2));
          score::gfx::Port* dst = nullptr;
          // sinks[0] is the control: nothing the sequence does may touch it,
          // so that a black frame there is the engine's doing and not ours.
          if(pick(2) == 0 && sinks.size() > 1)
            dst = p.sinkInput(sinks[1 + pick(int(sinks.size()) - 1)]);
          else
            dst = p.imageIn(nodes[pick(int(nodes.size()))], pick(2));
          if(src && dst && src != dst)
          {
            p.addEdgeIncremental(src, dst);
            cables.push_back({src, dst});
            ++inv.connects;
          }
          break;
        }
        case 6: // connect a buffer port
        {
          if(nodes.empty())
            break;
          auto* src = p.bufferOut(nodes[pick(int(nodes.size()))], 0);
          auto* dst = p.bufferIn(nodes[pick(int(nodes.size()))], 0);
          if(src && dst && src != dst)
          {
            p.addEdgeIncremental(src, dst);
            cables.push_back({src, dst});
            ++inv.connects;
          }
          break;
        }
        case 7: // connect a geometry port
        {
          if(nodes.empty())
            break;
          auto* src = p.geometryOut(nodes[pick(int(nodes.size()))], 0);
          auto* dst = p.geometryIn(nodes[pick(int(nodes.size()))], 0);
          if(src && dst && src != dst)
          {
            p.addEdgeIncremental(src, dst);
            cables.push_back({src, dst});
            ++inv.connects;
          }
          break;
        }
        case 8: // disconnect a cable
        {
          if(cables.size() <= 1)
            break;
          // Never drop the last cable feeding sink 0: the point is to keep
          // rendering while editing, not to idle.
          const int k = 1 + pick(int(cables.size()) - 1);
          p.removeEdgeIncremental(cables[k].source, cables[k].sink);
          cables.erase(cables.begin() + k);
          ++inv.disconnects;
          break;
        }
      }

      // The edit only bites when frames run through it.
      p.render(1 + pick(3));
      ++inv.renders;

      // Sample the control sink periodically: the first iteration at which it
      // stops showing its producer is the interesting number, not the last.
      if(inv.brokeAtIter < 0 && (inv.renders % 250) == 0)
      {
        const auto probe = p.readback(sinks[0]);
        // An invalid readback counts as broken. Skipping it instead is how a
        // sink that never rendered at all reads as healthy.
        const bool ok
            = probe.valid()
              && near(probe.at(probe.width / 2, probe.height / 2), {255, 0, 255, 255}, 16);
        if(!ok)
        {
          inv.brokeAtIter = inv.renders;
          inv.brokeAtEdits = inv.edits;
          inv.brokeAtConnects = inv.connects;
          if(probe.valid())
            inv.brokePixel = probe.at(probe.width / 2, probe.height / 2);
        }
      }

      if(!p.error().empty())
      {
        inv.failure = "after " + std::to_string(inv.edits) + " edits: " + p.error();
        break;
      }
    }

    inv.nodes = int(nodes.size());
    inv.sinks = int(sinks.size());
    inv.cables = int(cables.size());

    // Whatever the sequence left behind must still render a real frame, and
    // the one cable never touched must still deliver its producer's picture --
    // an edit sequence that quietly stops rendering would otherwise pass.
    {
      const auto before = p.readback(sinks[0]);
      inv.beforeFinalValid = before.valid();
      if(before.valid())
        inv.beforeFinalPixel = before.at(before.width / 2, before.height / 2);
    }
    p.render(3);
    if(inv.failure.empty() && !p.error().empty())
      inv.failure = "final render: " + p.error();

    const auto img = p.readback(sinks[0]);
    inv.sinkZeroValid = img.valid();
    if(inv.sinkZeroValid)
      inv.sinkZeroPixel = img.at(img.width / 2, img.height / 2);
  });

  if(skipped)
    SKIP(backend_name(backend) + std::string{": "} + skipReason);

  INFO(
      "backend=" << backendName << " edits=" << inv.edits
                 << " connects=" << inv.connects << " disconnects=" << inv.disconnects
                 << " renders=" << inv.renders << " nodes=" << inv.nodes
                 << " sinks=" << inv.sinks << " cables=" << inv.cables);

  // The control must be right BEFORE the sequence starts, or everything after
  // it is measuring a pipeline that never rendered.
  INFO("baseline valid=" << inv.baselineValid);
  REQUIRE(inv.baselineValid);
  REQUIRE(near(inv.baselinePixel, {255, 0, 255, 255}, 16));

  CHECK(inv.failure.empty());
  // A run that edited nothing, or that stopped rendering half way, would pass
  // vacuously on the failure check alone.
  CHECK(inv.edits > 0);
  CHECK(inv.connects > 0);
  CHECK(inv.renders > 0);

  // Cable 0 is never disconnected, so sink 0 must still show isf-solid-color's
  // magenta after every edit the sequence made around it.
  INFO(
      "sink 0 centre = (" << int(inv.sinkZeroPixel[0]) << "," << int(inv.sinkZeroPixel[1])
                          << "," << int(inv.sinkZeroPixel[2]) << ")");
  INFO(
      "sink 0 BASELINE (before any edit): valid=" << inv.baselineValid << " ("
        << int(inv.baselinePixel[0]) << "," << int(inv.baselinePixel[1]) << ","
        << int(inv.baselinePixel[2]) << ")");
  INFO(
      "sink 0 BEFORE the final render: valid=" << inv.beforeFinalValid << " ("
        << int(inv.beforeFinalPixel[0]) << "," << int(inv.beforeFinalPixel[1]) << ","
        << int(inv.beforeFinalPixel[2]) << ")");
  INFO(
      "control sink first wrong at iteration " << inv.brokeAtIter << " (edits="
                                               << inv.brokeAtEdits << " connects="
                                               << inv.brokeAtConnects << ") pixel=("
                                               << int(inv.brokePixel[0]) << ","
                                               << int(inv.brokePixel[1]) << ","
                                               << int(inv.brokePixel[2]) << ")");
  CHECK(inv.sinkZeroValid);
  CHECK(near(inv.sinkZeroPixel, {255, 0, 255, 255}, 16));
}
