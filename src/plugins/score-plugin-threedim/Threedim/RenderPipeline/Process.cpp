#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/ISFProcess.hpp>
#include <Gfx/TexturePort.hpp>

#include <QFileInfo>
#include <QImageReader>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::RenderPipeline::Model)
namespace Gfx::RenderPipeline
{

Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);

  init();
}

Model::~Model() = default;

void Model::initDefaultPorts() { }

void Model::init()
{
  if(m_inlets.empty() && m_outlets.empty())
  {
    const auto defaultVert = QStringLiteral(R"_(
void main()
{
  gl_Position = clipSpaceCorrMatrix * vec4(position, 0.0, 1.0);
  isf_FragNormCoord = vec2((gl_Position.x + 1.0) / 2.0, (gl_Position.y + 1.0) / 2.0);

#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_");
    const auto defaultFrag = QStringLiteral(R"_(/*{
    "CREDIT": "jm",
    "ISFVSN": "2",
    "MODE": "RAW_RASTER_PIPELINE",
    "CATEGORIES": [
      "Color Adjustment"
    ],
    "VERTEX_INPUTS": [
     { "LOCATION": 0, "TYPE": "vec2", "NAME": "position" },
     { "LOCATION": 1, "TYPE": "vec2", "NAME": "texcoord" }
    ],
    "VERTEX_OUTPUTS": [
     { "LOCATION": 0, "TYPE": "vec2", "NAME": "isf_FragNormCoord" }
    ],
    "FRAGMENT_INPUTS": [
     { "LOCATION": 0, "TYPE": "vec2", "NAME": "isf_FragNormCoord" }
    ],
    "FRAGMENT_OUTPUTS": [
     { "LOCATION": 0, "TYPE": "vec4", "NAME": "isf_FragColor" }
    ],
    "INPUTS": [
      {
        "NAME": "inputImage",
        "TYPE": "image"
      },
      {
        "NAME": "bright",
        "TYPE": "float",
        "MIN": -1.0,
        "MAX": 1.0,
        "DEFAULT": 0.0
      }
    ]
  }*/


void main()
{
  isf_FragColor = vec4(0, 1, 0, 1);
}
)_");

    (void)setProgram(
        {ShaderSource::ProgramType::RawRasterPipeline, defaultVert, defaultFrag});
  }
}

QString Model::prettyName() const noexcept
{
  return tr("Render Pipeline");
}

bool Model::validate(const std::vector<QString>& txt) const noexcept
{
  ShaderSource src{txt};
  src.type = isf::parser::ShaderType::RawRasterPipeline;
  const auto& [_, error] = ProgramCache::instance().get(src);
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

Process::ScriptChangeResult Model::setProgram(ShaderSource f)
{
  f.type = ProcessedProgram::ProgramType::RawRasterPipeline;
  setVertex(f.vertex);
  setFragment(f.fragment);
  if(const auto& [processed, error] = ProgramCache::instance().get(f); bool(processed))
  {
    ossia::flat_map<QString, ossia::value> previous_values;
    for(auto inl : m_inlets)
      if(auto control = qobject_cast<Process::ControlInlet*>(inl))
        previous_values.emplace(control->name(), control->value());

    auto inls = score::clearAndDeleteLater(m_inlets);
    auto outls = score::clearAndDeleteLater(m_outlets);

    m_processedProgram = *processed;

    qDebug() << (int)f.type << (int)processed->type;
    //    initDefaultPorts();

    m_inlets.push_back(new GeometryInlet{"Geometry In", Id<Process::Port>(1000), this});
    m_outlets.push_back(new TextureOutlet{"Texture Out", Id<Process::Port>(1000), this});

    ISFHelpers::setupISFModelPorts(
        *this, m_processedProgram.descriptor, previous_values);

    return {.valid = true, .inlets = std::move(inls), .outlets = std::move(outls)};
  }
  else
  {
    qDebug() << "Error while processing program: " << error;
  }
  return {};
}

Process::Descriptor ProcessFactory::descriptor(QString path) const noexcept
{
  return ISFHelpers::descriptorFromISFFile<RenderPipeline::Model>(path);
}
}
template <>
void DataStreamReader::read(const Gfx::RenderPipeline::Model& proc)
{
  m_stream << proc.m_program;

  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::RenderPipeline::Model& proc)
{
  Gfx::ShaderSource s;
  m_stream >> s;
  s.type = isf::parser::ShaderType::RawRasterPipeline;
  (void)proc.setProgram(s);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::RenderPipeline::Model& proc)
{
  obj["Vertex"] = proc.vertex();
  obj["Fragment"] = proc.fragment();

  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::RenderPipeline::Model& proc)
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
