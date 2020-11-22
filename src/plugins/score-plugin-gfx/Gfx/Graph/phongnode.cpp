#include "phongnode.hpp"

const char* frag = R"_(#version 450
vec4 lightPosition = vec4(100, 10, 10, 0.); // should be in the eye space
vec4 lightAmbient = vec4(0.1, 0.1, 0.1, 1); // light ambient color
vec4 lightDiffuse = vec4(0.0, 0.2, 0.7, 1); // light diffuse color
vec4 lightSpecular = vec4(0.9, 0.9, 0.9, 1); // light specular color
vec4 materialAmbient= vec4(0.1, 0.4, 0, 1); // material ambient color
vec4 materialDiffuse= vec4(0.2, 0.8, 0, 1); // material diffuse color
vec4 materialSpecular= vec4(0, 0, 1, 1); // material specular color
float materialShininess = 0.5; // material specular shininess
// uniform sampler2D map0; // texture map #1

layout(std140, binding = 1) uniform process_t {
  float time;
  float timeDelta;
  float progress;

  int passIndex;
  int frameIndex;

  vec4 date;
  vec4 mouse;
  vec4 channelTime;

  float sampleRate;
};
layout(location = 0) in vec3 esVertex;
layout(location = 1) in vec3 esNormal;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec4 fragColor;
void main()
{
    vec3 normal = normalize(esNormal);
    vec3 light;
    lightPosition.y = sin(time * 10) * 20.;
    lightPosition.z = cos(time * 10) * 50.;
    if(lightPosition.w == 0.0)
    {
        light = normalize(lightPosition.xyz);
    }
    else
    {
        light = normalize(lightPosition.xyz - esVertex);
    }
    vec3 view = normalize(-esVertex);
    vec3 halfv = normalize(light + view);

    vec3 color = lightAmbient.rgb * materialAmbient.rgb;        // begin with ambient
    float dotNL = max(dot(normal, light), 0.0);
    color += lightDiffuse.rgb * materialDiffuse.rgb * dotNL;    // add diffuse
    // color *= texture2D(map0, texCoord0).rgb;                    // modulate texture map
    float dotNH = max(dot(normal, halfv), 0.0);
    color += pow(dotNH, materialShininess) * lightSpecular.rgb * materialSpecular.rgb; // add specular

    // set frag color
    fragColor = vec4(color, materialDiffuse.a);
})_";

PhongNode::PhongNode(const Mesh* mesh) : m_mesh{mesh}
{
  QMatrix4x4 model;
  QMatrix4x4 projection;
  projection.perspective(90, 16. / 9., 0.001, 100.);
  QMatrix4x4 view;
  view.lookAt(QVector3D{0, 0, 1}, QVector3D{0, 0, 0}, QVector3D{0, 1, 0});
  QMatrix4x4 mv = view * model;
  QMatrix4x4 mvp = projection * mv;
  QMatrix3x3 norm = model.normalMatrix();

  setShaders(mesh->defaultVertexShader(), frag);
  const int sz = sizeof(ModelCameraUBO);
  m_materialData.reset(new char[sz]);
  std::fill_n(m_materialData.get(), sz, 0);
  char* cur = m_materialData.get();
  ModelCameraUBO* mc = reinterpret_cast<ModelCameraUBO*>(cur);
  model.copyDataTo(mc->model);
  projection.copyDataTo(mc->projection);
  view.copyDataTo(mc->view);
  mv.copyDataTo(mc->mv);
  mvp.copyDataTo(mc->mvp);
  norm.copyDataTo(mc->modelNormal);

  // translation, rotation, scale, camera => implies mvp matrices, etc
  input.push_back(new Port{this, cur, Types::Camera, {}});

  output.push_back(new Port{this, {}, Types::Image, {}});
}

PhongNode::~PhongNode() { }

const Mesh& PhongNode::mesh() const noexcept
{
  return *this->m_mesh;
}

struct RenderedPhongNode : RenderedNode
{
  using RenderedNode::RenderedNode;

  void customInit(Renderer& renderer) override { }

  void customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) override { }

  void customRelease(Renderer& renderer) override { }
};

score::gfx::NodeRenderer* PhongNode::createRenderer() const noexcept
{
  return NodeModel::createRenderer();
  // return new RenderedPhongNode{*this};
}
