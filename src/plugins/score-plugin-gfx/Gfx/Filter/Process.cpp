#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/PresetHelpers.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/DeleteAll.hpp>

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

  setProgram({QByteArray{}, defaultFrag});
}

Model::Model(
    const TimeVal& duration, const QString& init, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(1), this});

  setProgram(programFromFragmentShaderPath(init, {}));
}

Model::~Model() { }

bool Model::validate(const ShaderSource& txt) const noexcept
{
  score::gfx::GraphicsApi api = score::gfx::GraphicsApi::Vulkan;
  QShaderVersion version = QShaderVersion(100);
  const auto& [_, error] = ProgramCache::instance().get(api, version, txt);
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

void Model::setProgram(const ShaderSource& f)
{
  score::gfx::GraphicsApi api = score::gfx::GraphicsApi::Vulkan;
  QShaderVersion version = QShaderVersion(100);

  setVertex(f.vertex);
  setFragment(f.fragment);
  if(const auto& [processed, error] = ProgramCache::instance().get(api, version, f);
     bool(processed))
  {
    auto inls = score::clearAndDeleteLater(m_inlets);
    m_processedProgram = *processed;

    setupIsf(m_processedProgram.descriptor);
    inletsChanged();
    programChanged(m_program);
  }
}

void Model::loadPreset(const Process::Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();
  if(!obj.HasMember("Fragment") || !obj.HasMember("Vertex"))
    return;
  auto frag = obj["Fragment"].GetString();
  auto vert = obj["Vertex"].GetString();
  this->setProgram(ShaderSource{vert, frag});

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

void Model::setupIsf(const isf::descriptor& desc)
{
  /*
  {
    auto& [shader, error] = score::gfx::ShaderCache::get(
        m_processedProgram.vertex.toLatin1(), QShader::Stage::VertexStage);
    SCORE_ASSERT(error.isEmpty());
  }
  {
    auto& [shader, error] = score::gfx::ShaderCache::get(
        m_processedProgram.fragment.toLatin1(), QShader::Stage::FragmentStage);
    SCORE_ASSERT(error.isEmpty());
  }
  */

  int i = 0;
  using namespace isf;
  struct input_vis
  {
    const isf::input& input;
    const int i;
    Model& self;

    Process::Inlet* operator()(const float_input& v)
    {
      auto port = new Process::FloatSlider(
          v.min, v.max, v.def, QString::fromStdString(input.name), Id<Process::Port>(i),
          &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const long_input& v)
    {
      std::vector<std::pair<QString, ossia::value>> alternatives;
      std::size_t value_idx = 0;
      for(; value_idx < v.values.size() && value_idx < v.labels.size(); value_idx++)
      {
        alternatives.emplace_back(
            QString::fromStdString(v.labels[value_idx]), (int)v.values[value_idx]);
      }

      // If there are more values than labels:
      for(; value_idx < v.values.size(); value_idx++)
      {
        int val = (int)v.values[value_idx];
        alternatives.emplace_back(QString::number(val), val);
      }

      auto port = new Process::ComboBox(
          std::move(alternatives), (int)v.def, QString::fromStdString(input.name),
          Id<Process::Port>(i), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const event_input& v)
    {
      auto port = new Process::Button(
          QString::fromStdString(input.name), Id<Process::Port>(i), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }
    Process::Inlet* operator()(const bool_input& v)
    {
      auto port = new Process::Toggle(
          v.def, QString::fromStdString(input.name), Id<Process::Port>(i), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }
    Process::Inlet* operator()(const point2d_input& v)
    {
      ossia::vec2f min{0., 0.};
      ossia::vec2f max{1., 1.};
      ossia::vec2f init{0.5, 0.5};
      if(v.def)
        std::copy_n(v.def->begin(), 2, init.begin());
      if(v.min)
        std::copy_n(v.min->begin(), 2, min.begin());
      if(v.max)
        std::copy_n(v.max->begin(), 2, max.begin());
      auto port = new Process::XYSlider{
          min,  max, init, QString::fromStdString(input.name), Id<Process::Port>(i),
          &self};

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const point3d_input& v)
    {
      auto port = new Process::ControlInlet{Id<Process::Port>(i), &self};

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const color_input& v)
    {
      ossia::vec4f init{0.5, 0.5, 0.5, 1.};
      if(v.def)
      {
        std::copy_n(v.def->begin(), 4, init.begin());
      }
      auto port = new Process::HSVSlider(
          init, QString::fromStdString(input.name), Id<Process::Port>(i), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }
    Process::Inlet* operator()(const image_input& v)
    {
      auto port = new Gfx::TextureInlet(Id<Process::Port>(i), &self);
      self.m_inlets.push_back(port);
      return port;
    }
    Process::Inlet* operator()(const audio_input& v)
    {
      auto port = new Process::AudioInlet(Id<Process::Port>(i), &self);
      self.m_inlets.push_back(port);
      return port;
    }
    Process::Inlet* operator()(const audioFFT_input& v)
    {
      auto port = new Process::AudioInlet(Id<Process::Port>(i), &self);
      self.m_inlets.push_back(port);
      return port;
    }
  };

  for(const isf::input& input : desc.inputs)
  {
    ossia::visit(input_vis{input, i, *this}, input.data);
    i++;
  }

  // m_inlets.push_back(new GeometryInlet{Id<Process::Port>(i), this});
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
  proc.setProgram(s);

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
  proc.setProgram(s);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
