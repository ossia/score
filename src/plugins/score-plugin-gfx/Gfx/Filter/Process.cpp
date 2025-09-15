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

W_OBJECT_IMPL(Gfx::Filter::Model)

namespace Gfx::Filter
{
Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(1), this});

  const auto defaultFrag = QStringLiteral(R"_(/*{
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

  (void)setProgram({ShaderSource::ProgramType::ISF, QByteArray{}, defaultFrag});
}

Model::Model(
    const TimeVal& duration, const QString& init, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(1), this});

  if(init.endsWith("fs") || init.endsWith("frag"))
  {
    (void)setProgram(programFromISFFragmentShaderPath(init, {}));
  }
  else if(init.endsWith("vs") || init.endsWith("vert"))
  {
    (void)setProgram(programFromVSAVertexShaderPath(init, {}));
  }
}

Model::~Model() { }

bool Model::validate(const ShaderSource& txt) const noexcept
{
  const auto& [_, error] = ProgramCache::instance().get(txt);
  if(!error.isEmpty())
  {
    this->errorMessage(error);
    return false;
  }
  return true;
}

void Model::setVertex(QString f)
{
  if(f == m_program.vertex)
    return;
  m_program.vertex = std::move(f);
  m_processedProgram.vertex.clear();

  vertexChanged(m_program.vertex);
}

void Model::setFragment(QString f)
{
  if(f == m_program.fragment)
    return;
  m_program.fragment = std::move(f);
  m_processedProgram.fragment.clear();

  fragmentChanged(m_program.fragment);
}

Process::ScriptChangeResult Model::setProgram(const ShaderSource& f)
{
  setVertex(f.vertex);
  setFragment(f.fragment);
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
    qDebug() << "Erroe while processingp rogram: " << error;
  }
  return {};
}

void Model::loadPreset(const Process::Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();
  if(!obj.HasMember("Fragment") || !obj.HasMember("Vertex"))
    return;
  ShaderSource::ProgramType type = ShaderSource::ProgramType::ISF;
  if(obj.HasMember("Type"))
    type = (ShaderSource::ProgramType)obj["Type"].GetInt();
  auto frag = obj["Fragment"].GetString();
  auto vert = obj["Vertex"].GetString();
  (void)this->setProgram(ShaderSource{type, vert, frag});

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
    r.obj["Fragment"] = this->m_program.fragment;
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
  return tr("GFX Filter");
}

Process::Descriptor ProcessFactory::descriptor(QString path) const noexcept
{
  return ISFHelpers::descriptorFromISFFile<Filter::Model>(path);
}
}

template <>
void DataStreamReader::read(const Gfx::ShaderSource& p)
{
  m_stream << p.vertex << p.fragment;
}

template <>
void DataStreamWriter::write(Gfx::ShaderSource& p)
{
  m_stream >> p.vertex >> p.fragment;
}

template <>
void DataStreamReader::read(const Gfx::Filter::Model& proc)
{
  m_stream << proc.m_program;

  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Filter::Model& proc)
{
  Gfx::ShaderSource s;
  m_stream >> s;
  s.type = isf::parser::ShaderType::ISF;
  (void)proc.setProgram(s);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Filter::Model& proc)
{
  obj["Vertex"] = proc.vertex();
  obj["Fragment"] = proc.fragment();

  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::Filter::Model& proc)
{
  Gfx::ShaderSource s;
  s.vertex = obj["Vertex"].toString();
  s.fragment = obj["Fragment"].toString();
  s.type = isf::parser::ShaderType::ISF;
  (void)proc.setProgram(s);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
