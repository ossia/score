// An ISF EVENT input fires for one frame. It fires on true (the inspector
// button) and, since the generic port writer ignores impulses, used not to
// fire on an impulse from a cable or a message. CPU only: no RHI.
#include <Gfx/Graph/ISFNode.hpp>

#include <isf.hpp>

#include <ossia/network/value/value.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
constexpr auto shader = R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [
    { "NAME": "fire", "TYPE": "event" },
    { "NAME": "level", "TYPE": "float", "DEFAULT": 0.5 }
  ]
}*/
void main() { gl_FragColor = vec4(fire ? 1. : level); }
)_";

int eventValue(score::gfx::ISFNode& n, int port)
{
  return *static_cast<int*>(n.input[port]->value);
}
}

TEST_CASE("An ISF event input fires on an impulse, for one frame", "[gfx][isf][event]")
{
  isf::parser p{"", shader, 450, isf::parser::ShaderType::ISF};
  score::gfx::ISFNode node{p.data(), QString::fromStdString(p.vertex()),
                           QString::fromStdString(p.fragment())};
  REQUIRE(node.input.size() >= 2);
  CHECK(eventValue(node, 0) == 0);

  node.process(0, ossia::value{ossia::impulse{}});
  CHECK(eventValue(node, 0) == 1);
  CHECK(node.resetEventPortsAfterFrame());
  CHECK(eventValue(node, 0) == 0);

  // true still fires, false does not
  node.process(0, ossia::value{true});
  CHECK(eventValue(node, 0) == 1);
  node.resetEventPortsAfterFrame();
  node.process(0, ossia::value{false});
  CHECK(eventValue(node, 0) == 0);

  // An impulse on a non-event port changes nothing
  const float before = *static_cast<float*>(node.input[1]->value);
  node.process(1, ossia::value{ossia::impulse{}});
  CHECK(*static_cast<float*>(node.input[1]->value) == before);
}
