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

W_OBJECT_IMPL(Gfx::GeometryFilter::Model)

namespace Gfx::GeometryFilter
{
Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_inlets.push_back(new GeometryInlet{Id<Process::Port>(0), this});
  m_outlets.push_back(new GeometryOutlet{Id<Process::Port>(1), this});

  setScript("");
}

Model::Model(
    const TimeVal& duration, const QString& init, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_inlets.push_back(new GeometryInlet{Id<Process::Port>(0), this});
  m_outlets.push_back(new GeometryOutlet{Id<Process::Port>(1), this});

  setScript(init);
}

Model::~Model() { }

bool Model::validate(const QString& txt) const noexcept
{
  /*
  score::gfx::GraphicsApi api = score::gfx::GraphicsApi::Vulkan;
  QShaderVersion version = QShaderVersion(100);
  const auto& [_, error] = ProgramCache::instance().get(api, version, txt);
  if(!error.isEmpty())
  {
    this->errorMessage(error);
    return false;
  }
*/
  return true;
}

static const auto defaultGeometryFilter = QStringLiteral(R"_(/*{
"CREDIT": "ossia score",
"ISFVSN": "2",
"DESCRIPTION": "Colorize",
"MODE": "GEOMETRY_FILTER",
"CATEGORIES": [ "Geometry Effect", "Utility" ],
"INPUTS": [
  {
    "NAME": "intensity",
    "TYPE": "float",
    "DEFAULT": 1.,
    "MIN": 0.,
    "MAX": 1.
  }
]
}*/

void process_vertex(inout vec3 position, inout vec3 normal, inout vec2 uv, inout vec3 tangent, inout vec4 color)
{
  position.xyz += this_filter.intensity + 10. * sin(TIME * 0.001 * gl_VertexIndex);
}
)_");
Process::ScriptChangeResult Model::setScript(const QString& f)
{
  // FIXME isn't called in that case?
  // if(f == m_script)
  //   return {};
  m_script = f;
  if(m_script.isEmpty())
    m_script = defaultGeometryFilter;
  QString processed = m_script;
  {
    auto inls = score::clearAndDeleteLater(m_inlets);
    try
    {
      m_inlets.push_back(new GeometryInlet{Id<Process::Port>(0), this});
      isf::parser p{processed.toStdString()};
      m_processedProgram.descriptor = p.data();
      m_processedProgram.shader = QString::fromStdString(p.geometry_filter());

      setupIsf(m_processedProgram.descriptor);
      return {.valid = true, .inlets = std::move(inls), .outlets = {}};
    }
    catch(...)
    {
    }
    return {.valid = false, .inlets = std::move(inls), .outlets = {}};
  }
  return {};
}

void Model::loadPreset(const Process::Preset& preset)
{
  /*
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
*/
}

Process::Preset Model::savePreset() const noexcept
{
  Process::Preset p;
  /*
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
*/
  return p;
}
QString Model::prettyName() const noexcept
{
  return tr("Geometry Filter");
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

  int i = 1;
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
      auto port = new Process::XYSpinboxes{min,
                                           max,
                                           init,
                                           false,
                                           QString::fromStdString(input.name),
                                           Id<Process::Port>(i),
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
void DataStreamReader::read(const Gfx::GeometryFilter::Model& proc)
{
  m_stream << proc.m_script;
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::GeometryFilter::Model& proc)
{
  QString s;
  m_stream >> s;
  proc.setScript(s);
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::GeometryFilter::Model& proc)
{
  obj["Script"] = proc.script();
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::GeometryFilter::Model& proc)
{
  QString s = obj["Script"].toString();
  proc.setScript(s);
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
