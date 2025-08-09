#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/PresetHelpers.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
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

  (void)setProgram({QByteArray{}, defaultFrag});
}

Model::Model(
    const TimeVal& duration, const QString& init, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  metadata().setInstanceName(*this);
  m_outlets.push_back(new TextureOutlet{Id<Process::Port>(1), this});

  (void)setProgram(programFromFragmentShaderPath(init, {}));
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

    setupIsf(m_processedProgram.descriptor, previous_values);
    return {.valid = true, .inlets = std::move(inls), .outlets = {}};
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
  auto frag = obj["Fragment"].GetString();
  auto vert = obj["Vertex"].GetString();
  (void)this->setProgram(ShaderSource{vert, frag});

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

void Model::setupIsf(
    const isf::descriptor& desc,
    const ossia::flat_map<QString, ossia::value>& previous_values)
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
    const ossia::flat_map<QString, ossia::value>& previous_values;
    const isf::input& input;
    const int i;
    Model& self;

    Process::Inlet* operator()(const float_input& v)
    {
      auto nm = QString::fromStdString(input.name);
      auto port = new Process::FloatSlider(
          v.min, v.max, v.def, QString::fromStdString(input.name), Id<Process::Port>(i),
          &self);

      self.m_inlets.push_back(port);
      if(auto it = previous_values.find(nm);
         it != previous_values.end() && it->second.get_type() == ossia::val_type::FLOAT)
        port->setValue(it->second);

      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const long_input& v)
    {
      auto nm = QString::fromStdString(input.name);

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
          std::move(alternatives), (int)v.def, nm, Id<Process::Port>(i), &self);

      if(auto it = previous_values.find(nm);
         it != previous_values.end()
         && it->second.get_type() == port->value().get_type())
        port->setValue(it->second);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const event_input& v)
    {
      auto nm = QString::fromStdString(input.name);
      auto port = new Process::Button(nm, Id<Process::Port>(i), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }
    Process::Inlet* operator()(const bool_input& v)
    {
      auto nm = QString::fromStdString(input.name);
      auto port = new Process::Toggle(v.def, nm, Id<Process::Port>(i), &self);

      if(auto it = previous_values.find(nm);
         it != previous_values.end()
         && it->second.get_type() == port->value().get_type())
        port->setValue(it->second);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }
    Process::Inlet* operator()(const point2d_input& v)
    {
      auto nm = QString::fromStdString(input.name);
      ossia::vec2f min{-100., -100.};
      ossia::vec2f max{100., 100.};
      ossia::vec2f init{0.0, 0.0};
      if(v.def)
        std::copy_n(v.def->begin(), 2, init.begin());
      if(v.min)
        std::copy_n(v.min->begin(), 2, min.begin());
      if(v.max)
        std::copy_n(v.max->begin(), 2, max.begin());
      auto port = new Process::XYSpinboxes{
          min, max, init, false, nm, Id<Process::Port>(i), &self};

      if(auto it = previous_values.find(nm);
         it != previous_values.end()
         && it->second.get_type() == port->value().get_type())
        port->setValue(it->second);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const point3d_input& v)
    {
      auto nm = QString::fromStdString(input.name);
      ossia::vec3f min{-100., -100., -100.};
      ossia::vec3f max{100., 100., 100.};
      ossia::vec3f init{0., 0., 0.};
      if(v.def)
        std::copy_n(v.def->begin(), 3, init.begin());
      if(v.min)
        std::copy_n(v.min->begin(), 3, min.begin());
      if(v.max)
        std::copy_n(v.max->begin(), 3, max.begin());
      auto port
          = new Process::XYZSpinboxes{min, max, init, nm, Id<Process::Port>(i), &self};

      if(auto it = previous_values.find(nm);
         it != previous_values.end()
         && it->second.get_type() == port->value().get_type())
        port->setValue(it->second);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
      return port;
    }

    Process::Inlet* operator()(const color_input& v)
    {
      auto nm = QString::fromStdString(input.name);
      ossia::vec4f init{0.5, 0.5, 0.5, 1.};
      if(v.def)
      {
        std::copy_n(v.def->begin(), 4, init.begin());
      }
      auto port = new Process::HSVSlider(
          init, QString::fromStdString(input.name), Id<Process::Port>(i), &self);

      if(auto it = previous_values.find(nm);
         it != previous_values.end()
         && it->second.get_type() == port->value().get_type())
        port->setValue(it->second);

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
    ossia::visit(input_vis{previous_values, input, i, *this}, input.data);
    i++;
  }

  // m_inlets.push_back(new GeometryInlet{Id<Process::Port>(i), this});
}

Process::Descriptor ProcessFactory::descriptor(QString path) const noexcept
{
  auto base = Metadata<Process::Descriptor_k, Filter::Model>::get();

  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return base;

  try
  {
    auto [_, desc] = isf::parser::parse_isf_header(score::readFileAsString(f));
    if(!desc.credits.empty())
      base.author = QString::fromStdString(desc.credits);
    if(!desc.description.empty())
      base.description = QString::fromStdString(desc.credits);
    for(auto& cat : desc.categories)
      base.tags.push_back(QString::fromStdString(cat));
  }
  catch(...)
  {
  }

  return base;
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
  (void)proc.setProgram(s);

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
