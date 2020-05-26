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
W_OBJECT_IMPL(Gfx::Mesh::Model)
namespace Gfx::Mesh
{

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

void Model::setFragment(QString f)
{
  if (f == m_fragment)
    return;
  m_fragment = f;

  for (auto inlet : m_inlets)
    delete inlet;
  m_inlets.clear();

  try
  {
    isf::parser p{{}, f.toStdString()};
    auto isfprocessed = QString::fromStdString(p.fragment());
    if (isfprocessed != f)
    {
      m_processedFragment = isfprocessed;
      m_isfDescriptor = p.data();
      setupIsf(m_isfDescriptor);

      inletsChanged();
      outletsChanged();

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
  outletsChanged();
}

void Model::setMesh(QString f)
{
  if (m_mesh != f)
  {
    m_mesh = f;
    meshChanged(m_mesh);
  }
}

QString Model::prettyName() const noexcept
{
  return tr("GFX Mesh");
}

void Model::setupIsf(const isf::descriptor& desc)
{
  int i = 0;
  using namespace isf;
  struct input_vis
  {
    const isf::input& input;
    const int i;
    Model& self;

    Process::Inlet* operator()(const float_input& v)
    {
      return new Process::FloatSlider(
          v.min, v.max, v.def, QString::fromStdString(input.name), Id<Process::Port>(i), &self);
    }

    Process::Inlet* operator()(const long_input& v)
    {
      std::vector<std::pair<QString, ossia::value>> alternatives;
      for (std::size_t i = 0; i < v.values.size() && i < v.labels.size(); i++)
      {
        alternatives.emplace_back(QString::fromStdString(v.labels[i]), (int)v.values[i]);
      }
      return new Process::ComboBox(
          std::move(alternatives),
          (int)v.def,
          QString::fromStdString(input.name),
          Id<Process::Port>(i),
          &self);
    }
    Process::Inlet* operator()(const event_input& v)
    {
      return new Process::Button(QString::fromStdString(input.name), Id<Process::Port>(i), &self);
    }
    Process::Inlet* operator()(const bool_input& v)
    {
      return new Process::Toggle(
          v.def, QString::fromStdString(input.name), Id<Process::Port>(i), &self);
    }
    Process::Inlet* operator()(const point2d_input& v)
    {
      return new Process::ControlInlet{Id<Process::Port>(i), &self};
    }
    Process::Inlet* operator()(const point3d_input& v)
    {
      return new Process::ControlInlet{Id<Process::Port>(i), &self};
    }
    Process::Inlet* operator()(const color_input& v)
    {
      ossia::vec4f init{0.5, 0.5, 0.5, 1.};
      if (v.def)
      {
        std::copy_n(v.def->begin(), 4, init.begin());
      }
      return new Process::HSVSlider(
          init, QString::fromStdString(input.name), Id<Process::Port>(i), &self);
    }
    Process::Inlet* operator()(const image_input& v)
    {
      return new Gfx::TextureInlet(Id<Process::Port>(i), &self);
    }
    Process::Inlet* operator()(const audio_input& v)
    {
      return new Process::AudioInlet(Id<Process::Port>(i), &self);
    }
    Process::Inlet* operator()(const audioFFT_input& v)
    {
      return new Process::AudioInlet(Id<Process::Port>(i), &self);
    }
  };

  for (const isf::input& input : desc.inputs)
  {
    auto inlet = std::visit(input_vis{input, i, *this}, input.data);
    if (inlet)
    {
      m_inlets.push_back(inlet);
      controlAdded(inlet->id());
    }
    i++;
  }
}

void Model::setupNormalShader()
{
  auto& [shader, error]
      = ShaderCache::get(m_processedFragment.toLatin1(), QShader::Stage::FragmentStage);

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
  return {"obj", "gltf"};
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"obj", "gltf"};
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
      p.creation.key = Metadata<ConcreteKey_k, Gfx::Mesh::Model>::get();
      p.creation.prettyName = QFileInfo{filename}.baseName();
      p.setup = [str = file](Process::ProcessModel& m, score::Dispatcher& disp) {
        auto& midi = static_cast<Gfx::Mesh::Model&>(m);
        disp.submit(new ChangeMesh{midi, QString{str}});
      };
      vec.push_back(std::move(p));
    }
  }
  return vec;
}
}
template <>
void DataStreamReader::read(const Gfx::Mesh::Model& proc)
{
  m_stream << proc.m_fragment << proc.m_mesh;
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Mesh::Model& proc)
{

  QString s;
  m_stream >> s;
  proc.setFragment(s);
  m_stream >> proc.m_mesh;
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Mesh::Model& proc)
{
  obj["Fragment"] = proc.fragment();
  obj["Mesh"] = proc.mesh();
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::Mesh::Model& proc)
{
  proc.setFragment(obj["Fragment"].toString());
  proc.setMesh(obj["Mesh"].toString());
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
