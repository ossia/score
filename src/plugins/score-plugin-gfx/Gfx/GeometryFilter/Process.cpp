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

  (void)setScript("");
}

Model::Model(
    const TimeVal& duration, const QString& init, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_inlets.push_back(new GeometryInlet{Id<Process::Port>(0), this});
  m_outlets.push_back(new GeometryOutlet{Id<Process::Port>(1), this});

  QFile f{init};
  if(f.open(QIODevice::ReadOnly))
    (void)setScript(f.readAll());
}

Model::~Model() { }

bool Model::validate(const QString& txt) const noexcept
{
  static constexpr auto default_vtx = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

// Time-dependent uniforms, only relevant during execution
layout(std140, binding = 1) uniform process_t {
  float TIME;
  float TIMEDELTA;
  float PROGRESS;

  int PASSINDEX;
  int FRAMEINDEX;

  vec2 RENDERSIZE;
  vec4 DATE;
  vec4 MOUSE;
  vec4 CHANNEL_TIME;

  float SAMPLERATE;
} isf_process_uniforms;

layout(std140, binding = 2) uniform material_t {
  mat4 matrixModelViewProjection;
  mat4 matrixModelView;
  mat4 matrixModel;
  mat4 matrixView;
  mat4 matrixProjection;
  mat3 matrixNormal;
} mat;

float TIME = isf_process_uniforms.TIME;
float TIMEDELTA = isf_process_uniforms.TIMEDELTA;
float PROGRESS = isf_process_uniforms.PROGRESS;
int PASSINDEX = isf_process_uniforms.PASSINDEX;
int FRAMEINDEX = isf_process_uniforms.FRAMEINDEX;
vec4 DATE = isf_process_uniforms.DATE;

%vtx_define_filters%

out gl_PerVertex {
vec4 gl_Position;
};

void main()
{
  vec3 in_position = vec3(0);
  vec3 in_normal = vec3(0);
  vec2 in_uv = vec2(0);
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  process_vertex_0(in_position, in_normal, in_uv, in_tangent, in_color);

  gl_Position.xyz = in_position;
}
)_";

  try
  {
    const QString processed = txt;

    // 1. Process the glsl and prefix all the functions
    std::string str = processed.toStdString();

    isf::parser p{str, isf::parser::ShaderType::GeometryFilter};
    auto res = p.geometry_filter();
    auto r = QString::fromStdString(res);
    r.replace("%next%", "4");
    r.replace("%node%", "0");

    QString vtx = default_vtx;
    vtx.replace("%vtx_define_filters%", r);


    auto [vertexS, vertexError] = score::gfx::ShaderCache::get(
        score::gfx::GraphicsApi::OpenGL, QShaderVersion(330),
        vtx.toUtf8(), QShader::VertexStage);
    if(!vertexError.isEmpty())
    {
      this->errorMessage(0, vertexError);
      return false;
    }
    return true;
  }
  catch(const std::exception& e)
  {
    this->errorMessage(0, e.what());
    return false;
  }
  catch(...)
  {
    this->errorMessage(0, "Unknown error");
    return false;
  }
  return false;
}

static const auto defaultGeometryFilter = QStringLiteral(R"_(/*{
"CREDIT": "ossia score",
"ISFVSN": "2",
"DESCRIPTION": "Example geometry effect",
"MODE": "GEOMETRY_FILTER",
"CATEGORIES": [ "Geometry Effect", "Utility" ],
"INPUTS": [
  {
    "NAME": "intensity",
    "TYPE": "float",
    "DEFAULT": 1.,
    "MIN": 0.,
    "MAX": 0.1
  }
]
}*/

void process_vertex(inout vec3 position, inout vec3 normal, inout vec2 uv, inout vec3 tangent, inout vec4 color)
{
  position.xyz += this_filter.intensity * 10. * sin(TIME * 0.001 * gl_VertexIndex);
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

      isf::parser p{processed.toStdString(), isf::parser::ShaderType::GeometryFilter};
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
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();
  if(!obj.HasMember("Shader"))
    return;

  (void)this->setScript(obj["Shader"].GetString());

  auto controls = obj["Controls"].GetArray();
  Process::loadFixedControls(controls, *this);

  programChanged();
}

Process::Preset Model::savePreset() const noexcept
{
  Process::Preset p;
  p.name = this->metadata().getName();
  p.key.key = this->concreteKey();

  JSONReader r;
  {
    r.stream.StartObject();
    r.obj["Shader"] = this->m_script;

    r.stream.Key("Controls");
    Process::saveFixedControls(r, *this);
    r.stream.EndObject();
  }

  p.data = r.toByteArray();
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
      if(v.labels.size() == v.values.size())
      {
        // Sane, respectful example
        for(std::size_t value_idx = 0; value_idx < v.values.size(); value_idx++)
        {
          auto& val = v.values[value_idx];
          if(auto int_ptr = ossia::get_if<int64_t>(&val))
          {
            alternatives.emplace_back(
                QString::fromStdString(v.labels[value_idx]), int(*int_ptr));
          }
          else if(auto dbl_ptr = ossia::get_if<double>(&val))
          {
            alternatives.emplace_back(
                QString::fromStdString(v.labels[value_idx]), int(*dbl_ptr));
          }
          else
          {
            alternatives.emplace_back(
                QString::fromStdString(v.labels[value_idx]), int(value_idx));
          }
        }
      }
      else
      {
        for(std::size_t value_idx = 0; value_idx < v.values.size(); value_idx++)
        {
          auto& val = v.values[value_idx];
          if(auto int_ptr = ossia::get_if<int64_t>(&val))
          {
            alternatives.emplace_back(QString::number(*int_ptr), int(*int_ptr));
          }
          else if(auto dbl_ptr = ossia::get_if<double>(&val))
          {
            alternatives.emplace_back(QString::number(*dbl_ptr), int(*dbl_ptr));
          }
          else if(auto str_ptr = ossia::get_if<std::string>(&val))
          {
            alternatives.emplace_back(QString::fromStdString(*str_ptr), int(value_idx));
          }
        }
      }

      if(alternatives.empty())
      {
        alternatives.emplace_back("0", 0);
        alternatives.emplace_back("1", 1);
        alternatives.emplace_back("2", 2);
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
      ossia::vec2f min{-100., -100.};
      ossia::vec2f max{100., 100.};
      ossia::vec2f init{0.0, 0.0};
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
      ossia::vec3f min{-100., -100., -100.};
      ossia::vec3f max{100., 100., 100.};
      ossia::vec3f init{0., 0., 0.};
      if(v.def)
        std::copy_n(v.def->begin(), 3, init.begin());
      if(v.min)
        std::copy_n(v.min->begin(), 3, min.begin());
      if(v.max)
        std::copy_n(v.max->begin(), 3, max.begin());
      auto port = new Process::XYZSpinboxes{min,
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
    Process::Inlet* operator()(const audioHist_input& v)
    {
      auto port = new Process::AudioInlet(Id<Process::Port>(i), &self);
      self.m_inlets.push_back(port);
      return port;
    }
    
    // CSF-specific input handlers
    Process::Inlet* operator()(const storage_input& v)
    {
      // Storage buffers are typically not user-controllable inputs
      // They're managed by the system, so we don't create a UI control
      return nullptr;
    }
    
    Process::Inlet* operator()(const texture_input& v)
    {
      auto port = new Gfx::TextureInlet(Id<Process::Port>(i), &self);
      self.m_inlets.push_back(port);
      return port;
    }
    
    Process::Inlet* operator()(const csf_image_input& v)
    {
      auto port = new Gfx::TextureInlet(Id<Process::Port>(i), &self);
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
  (void)proc.setScript(s);
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
  (void)proc.setScript(s);
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
