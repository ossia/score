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
"CREDIT": "ossia score",
"ISFVSN": "2",
"DESCRIPTION": "Colorize",
"CATEGORIES": [ "Color Effect", "Utility" ],
"INPUTS": [
  {
    "NAME": "inputImage",
    "TYPE": "image"
  },
  {
    "NAME": "color",
    "TYPE": "color",
    "DEFAULT": [
      0.8,
      0.4,
      0.2,
      1.
    ]
  }
]
}*/

void main() {
  vec4 srcPixel = IMG_THIS_PIXEL(inputImage);
  gl_FragColor = srcPixel * color;
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
  (void)proc.setProgram(s);

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
  (void)proc.setProgram(s);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
