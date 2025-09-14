#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/PresetHelpers.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/ISFProcess.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <QFileInfo>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::VSA::Model)

namespace Gfx::VSA
{
Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(1), this});

  const auto defaultVert = QStringLiteral(R"_(/*{
  "DESCRIPTION": "Intro Flowing Grid - A dynamic grid mesh with organic wave deformations",
  "CREDIT": "ossia score",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "CATEGORIES": [
    "Math",
    "Animated",
    "Grid",
    "Intro"
  ],
  "POINT_COUNT": 10000,
  "PRIMITIVE_MODE": "POINTS",
  "LINE_SIZE": "NATIVE",
  "BACKGROUND_COLOR": [
    0.02,
    0.02,
    0.05,
    1
  ],
  "INPUTS": [],
  "METADATA": {
    "DESCRIPTION": "A beautiful flowing grid that demonstrates dynamic mesh deformation"
  }
}*/

// Convert HSV to RGB for smooth color transitions
vec3 hsv2rgb(vec3 c) {
  c = vec3(c.x, clamp(c.yz, 0.0, 1.0));
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
  // Create a square grid
  float gridSize = 100.0; // 100x100 grid
  float col = mod(vertexId, gridSize);
  float row = floor(vertexId / gridSize);
  
  // Normalize grid coordinates to -1 to 1 range
  vec2 gridPos = vec2(col, row) / gridSize * 2.0 - 1.0;
  
  // Apply aspect ratio
  vec2 aspect = vec2(1.0, resolution.x / resolution.y);
  vec2 pos = gridPos * aspect * 0.9; // Scale down slightly
  
  // Calculate distance from center for radial effects
  float dist = length(gridPos);
  
  // Create multiple wave effects
  float wave1 = sin(dist * 8.0 - time * 2.0) * 0.1;
  float wave2 = cos(gridPos.x * 5.0 + time * 1.5) * sin(gridPos.y * 5.0 - time * 1.2) * 0.08;
  float wave3 = sin(length(gridPos - vec2(0.5)) * 12.0 - time * 3.0) * 0.05;
  
  // Combine waves for complex movement
  float zOffset = (wave1 + wave2 + wave3) * (1.0 - dist * 0.5);
  
  // Add spiral rotation
  float angle = atan(gridPos.y, gridPos.x);
  float spiralEffect = sin(angle * 3.0 + dist * 4.0 - time) * 0.1 * (1.0 - dist);
  
  // Apply deformation
  vec2 deformation = vec2(
    cos(angle + spiralEffect) * dist,
    sin(angle + spiralEffect) * dist
  );
  
  // Mix between grid position and deformed position
  float deformAmount = sin(time * 0.5) * 0.5 + 0.5;
  pos = mix(pos, deformation * aspect * 0.9, deformAmount);
  
  // Add mouse interaction - create a ripple effect from mouse position
  vec2 mouseEffect = mouse - pos;
  float mouseDist = length(mouseEffect);
  float ripple = sin(mouseDist * 20.0 - time * 5.0) * exp(-mouseDist * 3.0) * 0.1;
  pos += normalize(mouseEffect) * ripple;
  
  // Set position with Z displacement
  gl_Position = vec4(pos, zOffset * 0.5, 1.0);
  
  // Create dynamic color based on position and deformation
  float colorPhase = dist + time * 0.1;
  float hue = colorPhase * 0.5 + zOffset * 2.0;
  float saturation = 0.7 + wave1 * 0.3;
  float brightness = 0.8 + zOffset * 2.0;
  
  // Add edge highlighting
  float edgeFactor = 1.0 - smoothstep(0.8, 1.0, dist);
  brightness *= edgeFactor;
  
  // Set color
  vec3 color = hsv2rgb(vec3(hue, saturation, brightness));
  v_color = vec4(color, edgeFactor);
  
  // Dynamic point size based on deformation
  float pointSize = 2.0 + abs(zOffset) * 10.0;
  gl_PointSize = pointSize * edgeFactor;
}
)_");

  (void)setProgram(
      {ShaderSource::ProgramType::VertexShaderArt, defaultVert, QByteArray{}});
}

Model::Model(
    const TimeVal& duration, const QString& init, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(1), this});

  (void)setProgram(programFromVSAVertexShaderPath(init, {}));
}

Model::~Model() { }

bool Model::validate(const QString& txt) const noexcept
{
  return true;
  //  const auto& [_, error] = ProgramCache::instance().get(txt);
  //  if(!error.isEmpty())
  //  {
  //    this->errorMessage(error);
  //    return false;
  //  }
  //  return true;
}

Process::ScriptChangeResult Model::setVertex(QString f)
{
  if(f == m_program.vertex)
    return {};
  m_program.vertex = std::move(f);
  m_processedProgram.vertex.clear();

  vertexChanged(m_program.vertex);
  return setProgram(m_program);
}

Process::ScriptChangeResult Model::setProgram(const ShaderSource& f)
{
  m_program.vertex = f.vertex;
  m_program.fragment.clear();
  m_processedProgram.fragment.clear();
  if(const auto& [processed, error] = ProgramCache::instance().get(f); bool(processed))
  {
    ossia::flat_map<QString, ossia::value> previous_values;
    for(auto inl : m_inlets)
      if(auto control = qobject_cast<Process::ControlInlet*>(inl))
        previous_values.emplace(control->name(), control->value());

    auto inls = score::clearAndDeleteLater(m_inlets);
    m_processedProgram = *processed;

    ISFHelpers::setupISFModelPorts(
        *this, m_processedProgram.descriptor, previous_values);
    return {.valid = true, .inlets = std::move(inls), .outlets = {}};
  }
  else
  {
    qDebug() << "Error while processing program: " << error;
  }
  return {};
}

void Model::loadPreset(const Process::Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();
  if(!obj.HasMember("Vertex"))
    return;
  auto vert = obj["Vertex"].GetString();
  (void)this->setProgram(
      ShaderSource{ShaderSource::ProgramType::VertexShaderArt, vert, ""});

  auto controls = obj["Controls"].GetArray();
  Process::loadFixedControls(controls, *this);
}

Process::Preset Model::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key.key = this->concreteKey();

  JSONReader r;
  {
    r.stream.StartObject();
    r.obj["Vertex"] = this->m_program.vertex;

    r.stream.Key("Controls");
    Process::saveFixedControls(r, *this);
    r.stream.EndObject();
  }

  p.data = r.toByteArray();
  return p;
}

QString Model::prettyName() const noexcept
{
  return tr("Vertex Shader Art");
}

Process::Descriptor ProcessFactory::descriptor(QString path) const noexcept
{
  return ISFHelpers::descriptorFromISFFile<VSA::Model>(path);
}
}

template <>
void DataStreamReader::read(const Gfx::VSA::Model& proc)
{
  m_stream << proc.m_program;

  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::VSA::Model& proc)
{
  Gfx::ShaderSource s;
  m_stream >> s;
  (void)proc.setVertex(s.vertex);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::VSA::Model& proc)
{
  obj["Vertex"] = proc.vertex();

  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::VSA::Model& proc)
{
  Gfx::ShaderSource s;
  s.vertex = obj["Vertex"].toString();
  (void)proc.setVertex(s.vertex);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
