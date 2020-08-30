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
R"_(void main(void)	{
  isf_vertShaderInit();
}
)_");

Model::Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(0), this});

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

  setProgram({defaultISFVertex, defaultFrag});
}

Model::~Model() { }

bool Model::validate(const ShaderProgram& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

void Model::setVertex(QString f)
{
  if (f == m_program.vertex)
    return;
  m_program.vertex = f;
  m_processedProgram.vertex.clear();

  vertexChanged(f);
}

namespace
{
void updateToGlsl45(ShaderProgram& program)
{
  static const QRegularExpression out_expr{R"_(^out\s+(\w+)\s+(\w+)\s*;)_", QRegularExpression::MultilineOption};
  static const QRegularExpression in_expr{R"_(^in\s+(\w+)\s+(\w+)\s*;)_", QRegularExpression::MultilineOption};

  ossia::flat_map<QString, int> attributes_locations_map;

  // First fixup the vertex shader and look for all the attributes
  {
    // location 0 is taken by fragColor - we start at 1.
    int cur_location = 1;

    auto match_idx = program.vertex.indexOf(out_expr);
    while(match_idx != -1)
    {
      const QStringRef partialString = program.vertex.midRef(match_idx);
      const auto& match = out_expr.match(partialString);
      const int len = match.capturedLength(0);
      attributes_locations_map[match.captured(2)] = cur_location;

      program.vertex.insert(match_idx, QString("layout(location = %1) ").arg(cur_location));
      cur_location++;

      match_idx = program.vertex.indexOf(out_expr, match_idx + len);
    }
  }

  // Then move on to the fragment shader, and reuse the same locations.
  {
    auto match_idx = program.fragment.indexOf(in_expr);
    while(match_idx != -1)
    {
      const QStringRef partialString = program.fragment.midRef(match_idx);
      const auto& match = in_expr.match(partialString);
      const int len = match.capturedLength(0);

      const int loc = attributes_locations_map[match.captured(2)];

      program.fragment.insert(match_idx, QString("layout(location = %1) ").arg(loc));

      match_idx = program.fragment.indexOf(in_expr, match_idx + len);
    }
  }
}
}

void Model::setFragment(QString f)
{
  if (f == m_program.fragment)
    return;
  m_program.fragment = f;

  programChanged(m_program);
}

void Model::setProgram(const ShaderProgram& f)
{
  setVertex(f.vertex);
  setFragment(f.fragment);


  auto inls = std::move(m_inlets);
  m_inlets.clear();
  inletsChanged();

  for (auto inlet : inls)
    delete inlet;

  try
  {
    isf::parser p{m_program.vertex.toStdString(), m_program.fragment.toStdString()};
    auto isfVert = QString::fromStdString(p.vertex());
    auto isfFrag = QString::fromStdString(p.fragment());
    if (isfVert != m_program.vertex || isfFrag != m_program.fragment)
    {
      m_processedProgram.vertex = isfVert;
      m_processedProgram.fragment = isfFrag;

      updateToGlsl45(m_processedProgram);

      m_isfDescriptor = p.data();
      setupIsf(m_isfDescriptor);

      inletsChanged();
      programChanged(m_program);

      return;
    }
  }
  catch (...)
  {
  }

  m_isfDescriptor = {};
  m_processedProgram.vertex = m_program.vertex;
  m_processedProgram.fragment = m_program.fragment;
  setupNormalShader();

  inletsChanged();
  programChanged(m_program);
}

QString Model::prettyName() const noexcept
{
  return tr("GFX Filter");
}

void Model::setupIsf(const isf::descriptor& desc)
{
  auto& [shader, error]
      = ShaderCache::get(m_processedProgram.fragment.toLatin1(), QShader::Stage::FragmentStage);

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
      = ShaderCache::get(m_processedProgram.fragment.toLatin1(), QShader::Stage::FragmentStage);

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
    for (const auto& [filename, fragData] : data)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, Gfx::Filter::Model>::get();
      p.creation.prettyName = QFileInfo{filename}.baseName();

      // ISF works by storing a vertex shader next to the fragment shader.
      QString vertexName = filename;
      vertexName.replace(".frag", ".vert");
      vertexName.replace(".fs", ".vs");

      QString vertexData = defaultISFVertex;
      if(vertexName != filename)
      {
        if(QFile vertexFile{vertexName};
           vertexFile.exists() && vertexFile.open(QIODevice::ReadOnly))
        {
          vertexData = vertexFile.readAll();
        }
      }

      p.setup = [program = Gfx::ShaderProgram{vertexData, fragData}](Process::ProcessModel& m, score::Dispatcher& disp) {
        auto& midi = static_cast<Gfx::Filter::Model&>(m);
        disp.submit(new ChangeShader{midi, program});
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
void DataStreamReader::read(const Gfx::ShaderProgram& p)
{
  m_stream << p.vertex << p.fragment;
}

template <>
void DataStreamWriter::write(Gfx::ShaderProgram& p)
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
  Gfx::ShaderProgram s;
  m_stream >> s;
  proc.setProgram(s);

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
  obj["Vertex"] = proc.vertex();
  obj["Fragment"] = proc.fragment();

  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::Filter::Model& proc)
{
  Gfx::ShaderProgram s;
  s.vertex = obj["Vertex"].toString();
  s.fragment = obj["Fragment"].toString();
  proc.setProgram(s);

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
