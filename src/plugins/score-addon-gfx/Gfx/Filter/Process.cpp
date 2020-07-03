#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <QFileInfo>
#include <QShaderBaker>

#include <Gfx/Graph/node.hpp>
#include <Gfx/Graph/nodes.hpp>
#include <Gfx/Graph/shadercache.hpp>
#include <Gfx/TexturePort.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::Filter::Model)
namespace Gfx::Filter
{
static const QString defaultISFVertex = QStringLiteral(
R"_(
void main(void)	{
  isf_vertShaderInit();
}
)_");

Model::Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});

  setFragment(R"_(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

// Shared uniform buffer for the whole render window
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;

  vec2 renderSize;
};

// Time-dependent uniforms, only relevant during execution
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

// Everything here will be exposed as UI controls
layout(std140, binding = 2) uniform material_t {
  vec4 color;
};


void main()
{
  // Important : texture origin depends on the graphics API used (Vulkan, D3D, etc).
  // Thus the following adjustment is required :
  vec2 texcoord = vec2(v_texcoord.x, texcoordAdjust.y + texcoordAdjust.x * v_texcoord.y);

  fragColor = vec4(color.rgb * (1+sin(progress * 100))/2. * texcoord.xxy, 1.);
})_");
}

Model::~Model() { }

void Model::setVertex(QString f)
{
  if (f == m_vertex)
    return;
  m_vertex = f;
  m_processedVertex = f;
  if(m_processedVertex.contains("isf_vertShaderInit()"))
  {
    m_processedVertex.prepend(R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 isf_FragNormCoord;
void isf_vertShaderInit()
{
  gl_Position = vec4( position, 0.0, 1.0 );
  isf_FragNormCoord = vec2((gl_Position.x+1.0)/2.0, (gl_Position.y+1.0)/2.0);
}
)_");
  }

  vertexChanged(f);
}

void Model::setFragment(QString f)
{
  if (f == m_fragment)
    return;
  m_fragment = f;

  auto inls = std::move(m_inlets);
  m_inlets.clear();
  inletsChanged();

  for (auto inlet : inls)
    delete inlet;

  try
  {
    isf::parser p{{}, f.toStdString()};
    auto isfprocessed = QString::fromStdString(p.fragment());
    if (isfprocessed != f)
    {
      m_processedFragment = isfprocessed;
      if(m_vertex.isEmpty())
      {
        setVertex(defaultISFVertex);
      }
      m_isfDescriptor = p.data();
      setupIsf(m_isfDescriptor);

      inletsChanged();

      return;
    }
  }
  catch (...)
  {
  }

  m_isfDescriptor = {};
  m_processedFragment = m_fragment;
  setupNormalShader();

  inletsChanged();
}

QString Model::prettyName() const noexcept
{
  return tr("GFX Filter");
}

void Model::setupIsf(const isf::descriptor& desc)
{
  auto& [shader, error]
      = ShaderCache::get(m_processedFragment.toLatin1(), QShader::Stage::FragmentStage);

  if (!error.isEmpty())
  {
    errorMessage(0, error);
  }

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
          v.min, v.max, v.def, QString::fromStdString(input.name), Id<Process::Port>(i), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const long_input& v)
    {
      std::vector<std::pair<QString, ossia::value>> alternatives;
      for (std::size_t i = 0; i < v.values.size() && i < v.labels.size(); i++)
      {
        alternatives.emplace_back(QString::fromStdString(v.labels[i]), (int)v.values[i]);
      }
      auto port = new Process::ComboBox(
          std::move(alternatives),
          (int)v.def,
          QString::fromStdString(input.name),
          Id<Process::Port>(i),
          &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }
    Process::Inlet* operator()(const event_input& v)
    {
      auto port
          = new Process::Button(QString::fromStdString(input.name), Id<Process::Port>(i), &self);

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
      ossia::vec2f init{0.5, 0.5};
      if (v.def)
      {
        std::copy_n(v.def->begin(), 2, init.begin());
      }
      auto port = new Process::XYSlider{
          init, QString::fromStdString(input.name), Id<Process::Port>(i), &self};

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
      if (v.def)
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

  for (const isf::input& input : desc.inputs)
  {
    std::visit(input_vis{input, i, *this}, input.data);
    i++;
  }
}

void Model::setupNormalShader()
{
  auto& [shader, error]
      = ShaderCache::get(m_processedFragment.toLatin1(), QShader::Stage::FragmentStage);

  if (!error.isEmpty())
  {
    errorMessage(0, error);
  }

  int i = 0;
  const auto& d = shader.description();

  for (auto& ub : d.combinedImageSamplers())
  {
    m_inlets.push_back(new TextureInlet{Id<Process::Port>(i++), this});
  }

  for (auto& ub : d.uniformBlocks())
  {
    if (ub.blockName != "material_t")
      continue;

    for (auto& u : ub.members)
    {
      switch (u.type)
      {
        case QShaderDescription::Float:
          m_inlets.push_back(new Process::FloatSlider{Id<Process::Port>(i++), this});
          m_inlets.back()->hidden = true;
          m_inlets.back()->setCustomData(u.name);
          controlAdded(m_inlets.back()->id());
          break;
        case QShaderDescription::Vec4:
          if (!u.name.contains("_imgRect"))
          {
            m_inlets.push_back(new Process::HSVSlider{Id<Process::Port>(i++), this});
            m_inlets.back()->hidden = true;
            m_inlets.back()->setCustomData(u.name);
            controlAdded(m_inlets.back()->id());
          }
          break;
        default:
          m_inlets.push_back(new Process::ControlInlet{Id<Process::Port>(i++), this});
          m_inlets.back()->hidden = true;
          m_inlets.back()->setCustomData(u.name);
          controlAdded(m_inlets.back()->id());
          break;
      }
    }
  }
}

void Model::startExecution() { }

void Model::stopExecution() { }

void Model::reset() { }

void Model::setDurationAndScale(const TimeVal& newDuration) noexcept { }

void Model::setDurationAndGrow(const TimeVal& newDuration) noexcept { }

void Model::setDurationAndShrink(const TimeVal& newDuration) noexcept { }

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {}; // TODO
}

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"frag", "glsl", "fs"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"frag", "glsl", "fs"};
}

std::vector<Process::ProcessDropHandler::ProcessDrop> DropHandler::dropData(
    const std::vector<DroppedFile>& data,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<Process::ProcessDropHandler::ProcessDrop> vec;
  {
    for (const auto& [filename, file] : data)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Gfx::Filter::Model>::get();
      p.creation.prettyName = QFileInfo{filename}.baseName();
      p.setup = [str = file](Process::ProcessModel& m, score::Dispatcher& disp) {
        auto& midi = static_cast<Gfx::Filter::Model&>(m);
        disp.submit(new ChangeFragmentShader{midi, QString{str}});
      };
      vec.push_back(std::move(p));
    }
  }
  return vec;
}
}

/*
struct PortSaver
{
  Process::Inlets& originalInlets;
  Process::Inlets savedInlets;
  Process::Outlets& originalOutlets;
  Process::Outlets savedOutlets;
  PortSaver(Process::Inlets& i, Process::Outlets& o)
    : originalInlets{i}
    , savedInlets{std::move(i)}
    , originalOutlets{o}
    , savedOutlets{std::move(o)}
  {

  }

  ~PortSaver()
  {
    using namespace std;

    for(auto inlet  : originalInlets)
      delete inlet;
    swap(originalInlets, savedInlets);

    for(auto outlet  : originalOutlets)
      delete outlet;
    swap(originalOutlets, savedOutlets);
  }
};
*/
template <>
void DataStreamReader::read(const Gfx::Filter::Model& proc)
{
  m_stream << proc.m_fragment;

  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Filter::Model& proc)
{
  QString s;
  m_stream >> s;
  proc.setFragment(s);

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Filter::Model& proc)
{
  obj["Fragment"] = proc.fragment();

  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::Filter::Model& proc)
{
  proc.setFragment(obj["Fragment"].toString());

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
