// The render node's initial control values are the UI's (ISFProcess.hpp):
// without DEFAULT a float is its parsed default, a point 0, a colour
// (0.5, 0.5, 0.5, 1). The render node used (MAX - MIN) / 2 for a zero float and
// 0.5 for every missing point / colour component, so a preview -- or the first
// frame, before the executor's values arrive -- drew something else than the
// controls showed (shader board U13).
#include <Gfx/Graph/ISFNode.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QString>

#include <array>
#include <memory>
#include <cstring>

namespace
{
struct Node
{
  isf::descriptor desc;
  std::unique_ptr<score::gfx::ISFNode> node;
};

Node make(const std::string& inputs)
{
  isf::parser p{
      "",
      "/*{ \"ISFVSN\": \"2.0\", \"INPUTS\": [ " + inputs
          + " ] }*/\nvoid main() { gl_FragColor = vec4(0.0); }\n",
      450, isf::parser::ShaderType::ISF};
  return Node{
      p.data(), std::make_unique<score::gfx::ISFNode>(
                    p.data(), QString::fromStdString(p.vertex()),
                    QString::fromStdString(p.fragment()))};
}

template <std::size_t N>
std::array<float, N> values(const Node& n, int port)
{
  std::array<float, N> a;
  std::memcpy(a.data(), n.node->input[port]->value, sizeof(a));
  return a;
}

// The UI's rule (ISFProcess.hpp setupISFModelPorts).
template <std::size_t N, typename T>
std::array<float, N> ui(const T& in, std::array<float, N> fallback)
{
  if(!in.def)
    return fallback;
  std::array<float, N> a;
  for(std::size_t i = 0; i < N; i++)
    a[i] = (*in.def)[i];
  return a;
}
}

TEST_CASE("ISF render node defaults match the UI controls", "[gfx][isf][U13]")
{
  const auto n = make(R"_({ "NAME": "zero", "TYPE": "float", "MIN": -1.0, "MAX": 3.0, "DEFAULT": 0.0 },
                          { "NAME": "nodef", "TYPE": "float", "MIN": 2.0, "MAX": 10.0 },
                          { "NAME": "p2", "TYPE": "point2D" },
                          { "NAME": "p3", "TYPE": "point3D" },
                          { "NAME": "c", "TYPE": "color" })_");
  const auto& in = n.desc.inputs;
  CHECK(values<1>(n, 0)[0] == float(ossia::get<isf::float_input>(in[0].data).def));
  CHECK(values<1>(n, 0)[0] == 0.f);
  CHECK(values<1>(n, 1)[0] == float(ossia::get<isf::float_input>(in[1].data).def));
  CHECK(values<2>(n, 2) == ui<2>(ossia::get<isf::point2d_input>(in[2].data), {0.f, 0.f}));
  CHECK(values<3>(n, 3) == ui<3>(ossia::get<isf::point3d_input>(in[3].data), {0.f, 0.f, 0.f}));
  CHECK(values<4>(n, 4)
        == ui<4>(ossia::get<isf::color_input>(in[4].data), {0.5f, 0.5f, 0.5f, 1.f}));
}
