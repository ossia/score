// L3 regression guard — split/threedim finding #10 (AnimationPlayer Speed
// never advances / ping-pongs).
//
// The Speed integration gated on `t == m_prev_time` but then overwrote
// m_prev_time with the freshly-advanced t. With Time held at 0 and Speed set,
// playback ping-ponged 0 -> speed/60 -> 0 forever instead of accumulating. The
// fix keeps a dedicated m_playback_time accumulator decoupled from the change-
// detection value.
//
// This drives the node directly (no GPU) with a one-node scene animated by a
// single linear translation channel whose X maps t -> t over [0,10]. Leaving
// Time at 0 and Speed at 2, we tick N frames and read the OUTPUT scene's
// animated translation. Post-fix it advances monotonically; the pre-fix engine
// alternates it back to ~0 every other frame (RED). We assert against the
// public OUTPUT (not m_playback_time) so the test compiles against BOTH the
// fixed and reverted headers — only the assertion flips.

#include <Threedim/AnimationPlayer.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
// Build a scene: one node (id 42) with an identity scene_transform child, plus
// one animation clip translating node 42's X from 0 to 10 across t in [0,10].
std::shared_ptr<ossia::scene_state> makeAnimatedScene()
{
  auto root = std::make_shared<ossia::scene_node>();
  root->id.value = 42;

  ossia::scene_transform xf{};
  xf.rotation[3] = 1.f;
  xf.scale[0] = xf.scale[1] = xf.scale[2] = 1.f;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(xf);
  root->children = children;

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(root);

  auto anim = std::make_shared<ossia::animation_component>();
  anim->duration = 10.f;
  ossia::animation_channel ch;
  ch.target_node_id = 42;
  ch.target_path = ossia::animation_target::translation;
  ch.interpolation = ossia::animation_interpolation::linear;
  ch.times = std::make_shared<std::vector<float>>(std::vector<float>{0.f, 10.f});
  ch.values = std::make_shared<std::vector<float>>(
      std::vector<float>{0.f, 0.f, 0.f, 10.f, 0.f, 0.f});
  anim->channels.push_back(ch);

  auto anims
      = std::make_shared<std::vector<ossia::animation_component_ptr>>();
  anims->push_back(anim);

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = roots;
  st->animations = anims;
  return st;
}

// Pull the animated node's translation.x out of the emitted output scene.
float outputTranslationX(Threedim::AnimationPlayer& node)
{
  const auto& out = node.outputs.scene_out.scene.state;
  REQUIRE(out);
  REQUIRE(out->roots);
  REQUIRE(!out->roots->empty());
  const auto& n0 = (*out->roots)[0];
  REQUIRE(n0);
  REQUIRE(n0->children);
  REQUIRE(!n0->children->empty());
  const auto* tf = ossia::get_if<ossia::scene_transform>(&(*n0->children)[0]);
  REQUIRE(tf);
  return tf->translation[0];
}
} // namespace

TEST_CASE(
    "AnimationPlayer Speed advances playback monotonically",
    "[threedim][animation][f10]")
{
  Threedim::AnimationPlayer node;
  node.inputs.scene_in.scene.state = makeAnimatedScene();
  node.inputs.time.value = 0.f;   // Time held constant at 0
  node.inputs.speed.value = 2.f;  // Speed engaged (2x)
  node.inputs.loop.value = false;
  node.inputs.clip_index.value = -1;

  constexpr int frames = 12;
  std::vector<float> xs;
  xs.reserve(frames);
  for(int i = 0; i < frames; ++i)
  {
    node();
    xs.push_back(outputTranslationX(node));
  }

  // Strictly monotonic increase: each frame must be past the previous one.
  for(std::size_t i = 1; i < xs.size(); ++i)
    CHECK(xs[i] > xs[i - 1]);

  // And it must have genuinely progressed, not jittered at the very start.
  CHECK(xs.back() > xs.front() + 0.1f);
}
