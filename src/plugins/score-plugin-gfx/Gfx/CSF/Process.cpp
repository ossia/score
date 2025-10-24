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
W_OBJECT_IMPL(Gfx::CSF::Model)

namespace Gfx::CSF
{
static const auto defaultCSF = QStringLiteral(R"_(/*{
"DESCRIPTION": "Example compute shader effect",
"CREDIT": "ossia score", 
"ISFVSN": "2.0",
"CATEGORIES": ["Compute"],
"RESOURCES": [
  {
    "NAME": "inputImage",
    "TYPE": "image",
    "ACCESS": "read_only",
    "FORMAT": "RGBA8"
  },
  {
    "NAME": "outputImage",
    "TYPE": "image", 
    "ACCESS": "write_only",
    "FORMAT": "RGBA8"
  },
  {
    "NAME": "intensity",
    "TYPE": "float",
    "LABEL": "Intensity",
    "DEFAULT": 1.0,
    "MIN": 0.0,
    "MAX": 2.0
  }  
],
"PASSES": [{
  "LOCAL_SIZE": [16, 16, 1],
  "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" }
}]
}*/

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    
    vec4 color = imageLoad(inputImage, coord);
    color.rgb *= intensity;
    
    imageStore(outputImage, coord, color);
}
)_");

Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "CSF", parent}
{
  metadata().setInstanceName(*this);
  (void)setCompute(defaultCSF);
}

Model::Model(
    const TimeVal& duration, const QString& init, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "CSF", parent}
{
  metadata().setInstanceName(*this);

  QFile f{init};
  if(f.open(QIODevice::ReadOnly))
    (void)setCompute(f.readAll());
}

Model::~Model() { }

bool Model::validate(const QString& txt) const noexcept
{
  try
  {
    // Parse the CSF shader to extract metadata
    std::string str = txt.toStdString();
    isf::parser p{str, isf::parser::ShaderType::CSF};

    // Check if it's a valid CSF shader
    if(p.mode() != isf::descriptor::CSF)
    {
      this->errorMessage(0, "Not a valid CSF shader");
      return false;
    }

    auto desc = p.data();

    // Generate the compute shader code
    auto computeShader = QString::fromStdString(p.compute_shader());

    // Validate the compute shader compilation
    auto [shader, error] = score::gfx::ShaderCache::get(
        score::gfx::GraphicsApi::OpenGL, QShaderVersion(450), computeShader.toUtf8(),
        QShader::ComputeStage);

    if(!error.isEmpty())
    {
      qDebug() << p.compute_shader().c_str();
      this->errorMessage(0, error);
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

Process::ScriptChangeResult Model::setScript(const QString& f)
{
  m_compute = f;

  QString processed = m_compute;

  auto inls = score::clearAndDeleteLater(m_inlets);
  auto outls = score::clearAndDeleteLater(m_outlets);

  try
  {
    // Parse CSF shader
    isf::parser p{processed.toStdString(), isf::parser::ShaderType::CSF};
    m_processedProgram.descriptor = p.data();
    m_processedProgram.fragment = QString::fromStdString(p.compute_shader());
    m_processedProgram.type = isf::parser::ShaderType::CSF;

    setupCSF(m_processedProgram.descriptor);
    return {.valid = true, .inlets = std::move(inls), .outlets = std::move(outls)};
  }
  catch(const std::exception& e)
  {
    this->errorMessage(0, e.what());
  }
  catch(...)
  {
    this->errorMessage(0, "Unknown error parsing CSF shader");
  }

  return {.valid = false, .inlets = std::move(inls), .outlets = std::move(outls)};

  return {};
}

Process::ScriptChangeResult Model::setCompute(QString f)
{
  if(f != m_compute)
  {
    m_compute = f;
    computeChanged(f);
    programChanged();
  }
  return setScript(f);
}

void Model::loadPreset(const Process::Preset& preset)
{
  const rapidjson::Document doc = readJson(preset.data);
  if(!doc.IsObject())
    return;
  auto obj = doc.GetObject();
  if(!obj.HasMember("Compute"))
    return;

  (void)this->setScript(obj["Compute"].GetString());

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
    r.obj["Compute"] = this->m_compute;

    r.stream.Key("Controls");
    Process::saveFixedControls(r, *this);
    r.stream.EndObject();
  }

  p.data = r.toByteArray();
  return p;
}

QString Model::prettyName() const noexcept
{
  return tr("Compute Shader");
}

void Model::setupCSF(const isf::descriptor& desc)
{
  using namespace isf;
  struct port_vis
  {
    const isf::input& input;
    int& input_i;
    int& output_i;
    Model& self;

    void operator()(const float_input& v)
    {
      auto port = new Process::FloatSlider(
          v.min, v.max, v.def, QString::fromStdString(input.name),
          Id<Process::Port>(input_i++), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
    }

    void operator()(const long_input& v)
    {
      std::vector<std::pair<QString, ossia::value>> alternatives;
      if(v.labels.size() == v.values.size())
      {
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
          Id<Process::Port>(input_i++), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
    }

    void operator()(const event_input& v)
    {
      auto port = new Process::Button(
          QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
    }

    void operator()(const bool_input& v)
    {
      auto port = new Process::Toggle(
          v.def, QString::fromStdString(input.name), Id<Process::Port>(input_i++),
          &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
    }

    void operator()(const point2d_input& v)
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
                                           Id<Process::Port>(input_i++),
                                           &self};

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
    }

    void operator()(const point3d_input& v)
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
                                            Id<Process::Port>(input_i++),
                                            &self};

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
    }

    void operator()(const color_input& v)
    {
      ossia::vec4f init{0.5, 0.5, 0.5, 1.};
      if(v.def)
      {
        std::copy_n(v.def->begin(), 4, init.begin());
      }
      auto port = new Process::HSVSlider(
          init, QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);

      self.m_inlets.push_back(port);
      self.controlAdded(port->id());
    }

    void operator()(const image_input& v)
    {
      auto port = new Gfx::TextureInlet(
          QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);
      self.m_inlets.push_back(port);
    }

    void operator()(const audio_input& v)
    {
      auto port = new Process::AudioInlet(
          QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);

      self.m_inlets.push_back(port);
    }

    void operator()(const audioFFT_input& v)
    {
      auto port = new Process::AudioInlet(
          QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);

      self.m_inlets.push_back(port);
    }

    void operator()(const audioHist_input& v)
    {
      auto port = new Process::AudioInlet(
          QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);
      self.m_inlets.push_back(port);
    }

    void operator()(const storage_input& v)
    {
      if(v.access == "read_only")
      {
        auto port = new Gfx::TextureInlet(
            QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);
        self.m_inlets.push_back(port);
      }
      else if(v.access.contains("write"))
      {
        auto port = new Gfx::TextureOutlet(
            QString::fromStdString(input.name), Id<Process::Port>(output_i++), &self);
        self.m_outlets.push_back(port);

        auto size_inl = new Process::IntSpinBox{
            1,
            100000000,
            1024,
            QString::fromStdString(input.name) + " size",
            Id<Process::Port>(input_i++),
            &self};
        self.m_inlets.push_back(size_inl);
        self.controlAdded(size_inl->id());
      }
    }

    void operator()(const texture_input& v)
    {
      auto port = new Gfx::TextureInlet(
          QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);

      self.m_inlets.push_back(port);
    }

    void operator()(const csf_image_input& v)
    {
      if(v.access == "read_only")
      {
        auto port = new Gfx::TextureInlet(
            QString::fromStdString(input.name), Id<Process::Port>(input_i++), &self);

        self.m_inlets.push_back(port);
      }
      else if(v.access.contains("write"))
      {
        auto port = new Gfx::TextureOutlet(
            QString::fromStdString(input.name), Id<Process::Port>(output_i++), &self);

        self.m_outlets.push_back(port);
      }
    }
  };

  int input_i = 0;
  int output_i = 0;
  for(const isf::input& input : desc.inputs)
  {
    ossia::visit(port_vis{input, input_i, output_i, *this}, input.data);
  }
}

Process::Descriptor ProcessFactory::descriptor(QString) const noexcept
{
  return Metadata<Process::Descriptor_k, Model>::get();
}

}

template <>
void DataStreamReader::read(const Gfx::CSF::Model& proc)
{
  m_stream << proc.m_compute;
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::CSF::Model& proc)
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
void JSONReader::read(const Gfx::CSF::Model& proc)
{
  obj["Compute"] = proc.script();
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::CSF::Model& proc)
{
  QString s = obj["Compute"].toString();
  (void)proc.setScript(s);
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
