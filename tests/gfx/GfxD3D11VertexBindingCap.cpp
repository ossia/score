// Qt's D3D11 backend records at most 8 vertex buffer bindings and drops the
// rest, so a pipeline needing more reads zeroes on the tail. The resolver
// warns about it, naming the inputs that fall off -- on D3D11 only, and on
// the strict overload too, which is what buildPipeline uses and which carries
// no QRhi of its own.
//
// Nine streams, each read by the shader, go through the strict overload on
// every available backend. The warning must fire exactly when the backend is
// D3D11: on Linux that checks it stays quiet; on Windows it checks it speaks.
//
// Registration:
//   score_add_gfx_test(d3d11_vertex_binding_cap GfxD3D11VertexBindingCap.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QMutex>
#include <iterator>
#include <QStringList>

namespace
{
struct Stream
{
  const char* name;
  ossia::attribute_semantic semantic;
  decltype(ossia::geometry::attribute{}.format) format;
  int components;
};

constexpr Stream kStreams[] = {
    {"position", ossia::attribute_semantic::position,
     ossia::geometry::attribute::float3, 3},
    {"normal", ossia::attribute_semantic::normal,
     ossia::geometry::attribute::float3, 3},
    {"tangent", ossia::attribute_semantic::tangent,
     ossia::geometry::attribute::float4, 4},
    {"texcoord0", ossia::attribute_semantic::texcoord0,
     ossia::geometry::attribute::float2, 2},
    {"texcoord1", ossia::attribute_semantic::texcoord1,
     ossia::geometry::attribute::float2, 2},
    {"texcoord2", ossia::attribute_semantic::texcoord2,
     ossia::geometry::attribute::float2, 2},
    {"texcoord3", ossia::attribute_semantic::texcoord3,
     ossia::geometry::attribute::float2, 2},
    {"color0", ossia::attribute_semantic::color0,
     ossia::geometry::attribute::float4, 4},
    {"color1", ossia::attribute_semantic::color1,
     ossia::geometry::attribute::float4, 4},
};
constexpr int kStreamCount = int(std::size(kStreams));

ossia::geometry makeNineStreamGeometry()
{
  ossia::geometry geom;
  for(int i = 0; i < kStreamCount; ++i)
  {
    const auto& s = kStreams[i];
    geom.bindings.push_back(
        {uint32_t(4 * s.components), ossia::geometry::binding::per_vertex, 0});
    ossia::geometry::attribute a;
    a.binding = i;
    a.location = i;
    a.format = s.format;
    a.byte_offset = 0;
    a.semantic = s.semantic;
    geom.attributes.push_back(a);
  }
  return geom;
}

QString vertexShaderSource()
{
  static const char* types[] = {"", "float", "vec2", "vec3", "vec4"};
  QString src = QStringLiteral("#version 450\n");
  for(int i = 0; i < kStreamCount; ++i)
    src += QStringLiteral("layout(location=%1) in %2 %3;\n")
               .arg(i)
               .arg(QString::fromLatin1(types[kStreams[i].components]))
               .arg(QString::fromLatin1(kStreams[i].name));
  src += QStringLiteral(
      "layout(location=0) out vec4 v_sum;\n"
      "void main() {\n"
      "  v_sum = vec4(normal, 0.) + tangent + vec4(texcoord0 + texcoord1 + "
      "texcoord2 + texcoord3, 0., 0.) + color0 + color1;\n"
      "  gl_Position = vec4(position, 1.0);\n"
      "}\n");
  return src;
}

QMutex g_logMutex;
QStringList g_log;
QtMessageHandler g_previous = nullptr;

void capture(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  {
    QMutexLocker lock{&g_logMutex};
    g_log << msg;
  }
  if(g_previous)
    g_previous(type, ctx, msg);
}
}

TEST_CASE(
    "the D3D11 vertex-binding cap is reported by the strict resolver",
    "[gfx][gpu][vertex-input]")
{
  using namespace score::gfx;
  using namespace score::test::gfx;

  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool ran = false;
  bool isD3D11 = false;
  bool remapped = false;
  bool pipelineKnowsRhi = false;
  QStringList warnings;
  std::string error;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto state = createRenderState(api, QSize(16, 16), nullptr);
    if(!state || !state->rhi)
    {
      error = "RHI unavailable";
      if(state)
        state->destroy();
      return;
    }
    auto& rhi = *state->rhi;
    isD3D11 = rhi.backend() == QRhi::D3D11;
    {
      const auto shaders = makeShaders(
          *state, vertexShaderSource(),
          QStringLiteral(R"_(#version 450
layout(location=0) in vec4 v_sum;
layout(location=0) out vec4 frag;
void main() { frag = v_sum; }
)_"));

      std::unique_ptr<QRhiGraphicsPipeline> pipeline(rhi.newGraphicsPipeline());
      pipelineKnowsRhi = pipeline->rhi() == &rhi;
      pipeline->setShaderStages(
          {{QRhiShaderStage::Vertex, shaders.first},
           {QRhiShaderStage::Fragment, shaders.second}});

      const auto geom = makeNineStreamGeometry();
      {
        QRhiVertexInputLayout layout;
        QList<QRhiVertexInputBinding> bindings;
        for(const auto& b : geom.bindings)
          bindings.append(QRhiVertexInputBinding{quint32(b.byte_stride)});
        layout.setBindings(bindings.cbegin(), bindings.cend());
        pipeline->setVertexInputLayout(layout);
      }
      FallbackBindingPlan plan;
      {
        QMutexLocker lock{&g_logMutex};
        g_log.clear();
      }
      g_previous = qInstallMessageHandler(capture);
      remapped = remapPipelineVertexInputs(*pipeline, shaders.first, geom, &plan);
      qInstallMessageHandler(g_previous);
      g_previous = nullptr;
      {
        QMutexLocker lock{&g_logMutex};
        for(const auto& m : g_log)
          if(m.contains(QStringLiteral("D3D11 backend records only 8")))
            warnings << m;
      }
      ran = true;
    }
    state->destroy();
  });

  INFO(error);
  REQUIRE(ran);
  INFO(warnings.join(QStringLiteral("\n")).toStdString());
  CHECK(remapped);
  // The strict path reads the backend from here, having no QRhi of its own.
  CHECK(pipelineKnowsRhi);
  if(isD3D11)
  {
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().contains(QStringLiteral("color1")));
  }
  else
  {
    CHECK(warnings.isEmpty());
  }
}
