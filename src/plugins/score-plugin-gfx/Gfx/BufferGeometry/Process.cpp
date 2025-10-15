#include "Process.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Gfx/TexturePort.hpp>

#include <score/application/ApplicationComponents.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::BufferGeometry::Model)
namespace Gfx::BufferGeometry
{

Model::Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
    : Process::ProcessModel{duration, id, "gfxProcess", parent}
{
  // Create input ports for buffers (up to 4 texture inputs)
  int inlet_id = 0;
  for(int i = 0; i < 4; ++i)
  {
    auto port = new Gfx::TextureInlet{Id<Process::Port>(inlet_id++), this};
    port->setName(QString("Buffer %1").arg(i));
    m_inlets.push_back(port);
  }
  
  // Create control inputs for configuration
  m_inlets.push_back(
      new Process::IntSlider{"Vertex Count", Id<Process::Port>(inlet_id++), this});
  static_cast<Process::IntSlider*>(m_inlets.back())->setDomain(ossia::make_domain(1, 100000));
  static_cast<Process::IntSlider*>(m_inlets.back())->setValue(m_config.vertex_count);

  // Position attribute configuration
  m_inlets.push_back(
      new Process::IntSpinBox{-1, 15, m_config.position.location, "Position Location", Id<Process::Port>(inlet_id++), this});
  
  std::vector<std::pair<QString, ossia::value>> format_options = {
      {"Disabled", -1},
      {"Float", static_cast<int>(ossia::geometry::attribute::fp1)},
      {"Vec2", static_cast<int>(ossia::geometry::attribute::fp2)},
      {"Vec3", static_cast<int>(ossia::geometry::attribute::fp3)},
      {"Vec4", static_cast<int>(ossia::geometry::attribute::fp4)}
  };
  m_inlets.push_back(
      new Process::ComboBox{format_options, static_cast<int>(ossia::geometry::attribute::fp3), "Position Format", Id<Process::Port>(inlet_id++), this});
  
  m_inlets.push_back(
      new Process::IntSpinBox{0, 1000, static_cast<int>(m_config.position.offset), "Position Offset", Id<Process::Port>(inlet_id++), this});
  
  m_inlets.push_back(
      new Process::IntSpinBox{0, 100, static_cast<int>(m_config.position.stride), "Position Stride", Id<Process::Port>(inlet_id++), this});
      
  m_inlets.push_back(
      new Process::IntSpinBox{0, 3, m_config.position.buffer_index, "Position Buffer", Id<Process::Port>(inlet_id++), this});

  // Normal attribute configuration
  m_inlets.push_back(
      new Process::IntSpinBox{-1, 15, m_config.normal.location, "Normal Location", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::ComboBox{format_options, static_cast<int>(ossia::geometry::attribute::fp3), "Normal Format", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 1000, static_cast<int>(m_config.normal.offset), "Normal Offset", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 100, static_cast<int>(m_config.normal.stride), "Normal Stride", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 3, m_config.normal.buffer_index, "Normal Buffer", Id<Process::Port>(inlet_id++), this});

  // Texcoord0 attribute configuration
  m_inlets.push_back(
      new Process::IntSpinBox{-1, 15, m_config.texcoord0.location, "TexCoord0 Location", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::ComboBox{format_options, static_cast<int>(ossia::geometry::attribute::fp2), "TexCoord0 Format", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 1000, static_cast<int>(m_config.texcoord0.offset), "TexCoord0 Offset", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 100, static_cast<int>(m_config.texcoord0.stride), "TexCoord0 Stride", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 3, m_config.texcoord0.buffer_index, "TexCoord0 Buffer", Id<Process::Port>(inlet_id++), this});

  // Color attribute configuration
  m_inlets.push_back(
      new Process::IntSpinBox{-1, 15, m_config.color.location, "Color Location", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::ComboBox{format_options, static_cast<int>(ossia::geometry::attribute::fp4), "Color Format", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 1000, static_cast<int>(m_config.color.offset), "Color Offset", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 100, static_cast<int>(m_config.color.stride), "Color Stride", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 3, m_config.color.buffer_index, "Color Buffer", Id<Process::Port>(inlet_id++), this});

  // Tangent attribute configuration
  m_inlets.push_back(
      new Process::IntSpinBox{-1, 15, m_config.tangent.location, "Tangent Location", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::ComboBox{format_options, static_cast<int>(ossia::geometry::attribute::fp3), "Tangent Format", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 1000, static_cast<int>(m_config.tangent.offset), "Tangent Offset", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 100, static_cast<int>(m_config.tangent.stride), "Tangent Stride", Id<Process::Port>(inlet_id++), this});
  m_inlets.push_back(
      new Process::IntSpinBox{0, 3, m_config.tangent.buffer_index, "Tangent Buffer", Id<Process::Port>(inlet_id++), this});

  // Create geometry output port
  m_outlets.push_back(new Gfx::GeometryOutlet{Id<Process::Port>(inlet_id++), this});

  // Set up default configuration
  m_config.position.location = 0;
  m_config.position.format = ossia::geometry::attribute::fp3;
  m_config.position.stride = 12; // 3 floats
  
  m_config.normal.location = 1;
  m_config.normal.format = ossia::geometry::attribute::fp3;
  m_config.normal.stride = 12;
  
  m_config.texcoord0.location = 2;
  m_config.texcoord0.format = ossia::geometry::attribute::fp2;
  m_config.texcoord0.stride = 8; // 2 floats
  
  m_config.color.location = 4;
  m_config.color.format = ossia::geometry::attribute::fp4;
  m_config.color.stride = 16; // 4 floats

  m_config.vertex_count = 0;
}

Model::~Model() = default;

void Model::setVertexCount(int count)
{
  if(m_config.vertex_count != count)
  {
    m_config.vertex_count = count;
    vertexCountChanged(count);
    configurationChanged();
  }
}

void Model::setPositionLocation(int loc)
{
  if(m_config.position.location != loc)
  {
    m_config.position.location = loc;
    positionLocationChanged(loc);
    configurationChanged();
  }
}

void Model::setPositionFormat(int fmt)
{
  if(m_config.position.format != fmt)
  {
    m_config.position.format = fmt;
    positionFormatChanged(fmt);
    configurationChanged();
  }
}

void Model::setPositionOffset(int offset)
{
  if(m_config.position.offset != static_cast<uint32_t>(offset))
  {
    m_config.position.offset = static_cast<uint32_t>(offset);
    positionOffsetChanged(offset);
    configurationChanged();
  }
}

void Model::setPositionStride(int stride)
{
  if(m_config.position.stride != static_cast<uint32_t>(stride))
  {
    m_config.position.stride = static_cast<uint32_t>(stride);
    positionStrideChanged(stride);
    configurationChanged();
  }
}

void Model::setPositionBuffer(int buf)
{
  if(m_config.position.buffer_index != buf)
  {
    m_config.position.buffer_index = buf;
    positionBufferChanged(buf);
    configurationChanged();
  }
}

void Model::setNormalLocation(int loc)
{
  if(m_config.normal.location != loc)
  {
    m_config.normal.location = loc;
    normalLocationChanged(loc);
    configurationChanged();
  }
}

void Model::setNormalFormat(int fmt)
{
  if(m_config.normal.format != fmt)
  {
    m_config.normal.format = fmt;
    normalFormatChanged(fmt);
    configurationChanged();
  }
}

void Model::setNormalOffset(int offset)
{
  if(m_config.normal.offset != static_cast<uint32_t>(offset))
  {
    m_config.normal.offset = static_cast<uint32_t>(offset);
    normalOffsetChanged(offset);
    configurationChanged();
  }
}

void Model::setNormalStride(int stride)
{
  if(m_config.normal.stride != static_cast<uint32_t>(stride))
  {
    m_config.normal.stride = static_cast<uint32_t>(stride);
    normalStrideChanged(stride);
    configurationChanged();
  }
}

void Model::setNormalBuffer(int buf)
{
  if(m_config.normal.buffer_index != buf)
  {
    m_config.normal.buffer_index = buf;
    normalBufferChanged(buf);
    configurationChanged();
  }
}

void Model::setTexcoord0Location(int loc)
{
  if(m_config.texcoord0.location != loc)
  {
    m_config.texcoord0.location = loc;
    texcoord0LocationChanged(loc);
    configurationChanged();
  }
}

void Model::setTexcoord0Format(int fmt)
{
  if(m_config.texcoord0.format != fmt)
  {
    m_config.texcoord0.format = fmt;
    texcoord0FormatChanged(fmt);
    configurationChanged();
  }
}

void Model::setTexcoord0Offset(int offset)
{
  if(m_config.texcoord0.offset != static_cast<uint32_t>(offset))
  {
    m_config.texcoord0.offset = static_cast<uint32_t>(offset);
    texcoord0OffsetChanged(offset);
    configurationChanged();
  }
}

void Model::setTexcoord0Stride(int stride)
{
  if(m_config.texcoord0.stride != static_cast<uint32_t>(stride))
  {
    m_config.texcoord0.stride = static_cast<uint32_t>(stride);
    texcoord0StrideChanged(stride);
    configurationChanged();
  }
}

void Model::setTexcoord0Buffer(int buf)
{
  if(m_config.texcoord0.buffer_index != buf)
  {
    m_config.texcoord0.buffer_index = buf;
    texcoord0BufferChanged(buf);
    configurationChanged();
  }
}

void Model::setColorLocation(int loc)
{
  if(m_config.color.location != loc)
  {
    m_config.color.location = loc;
    colorLocationChanged(loc);
    configurationChanged();
  }
}

void Model::setColorFormat(int fmt)
{
  if(m_config.color.format != fmt)
  {
    m_config.color.format = fmt;
    colorFormatChanged(fmt);
    configurationChanged();
  }
}

void Model::setColorOffset(int offset)
{
  if(m_config.color.offset != static_cast<uint32_t>(offset))
  {
    m_config.color.offset = static_cast<uint32_t>(offset);
    colorOffsetChanged(offset);
    configurationChanged();
  }
}

void Model::setColorStride(int stride)
{
  if(m_config.color.stride != static_cast<uint32_t>(stride))
  {
    m_config.color.stride = static_cast<uint32_t>(stride);
    colorStrideChanged(stride);
    configurationChanged();
  }
}

void Model::setColorBuffer(int buf)
{
  if(m_config.color.buffer_index != buf)
  {
    m_config.color.buffer_index = buf;
    colorBufferChanged(buf);
    configurationChanged();
  }
}

void Model::setTangentLocation(int loc)
{
  if(m_config.tangent.location != loc)
  {
    m_config.tangent.location = loc;
    tangentLocationChanged(loc);
    configurationChanged();
  }
}

void Model::setTangentFormat(int fmt)
{
  if(m_config.tangent.format != fmt)
  {
    m_config.tangent.format = fmt;
    tangentFormatChanged(fmt);
    configurationChanged();
  }
}

void Model::setTangentOffset(int offset)
{
  if(m_config.tangent.offset != static_cast<uint32_t>(offset))
  {
    m_config.tangent.offset = static_cast<uint32_t>(offset);
    tangentOffsetChanged(offset);
    configurationChanged();
  }
}

void Model::setTangentStride(int stride)
{
  if(m_config.tangent.stride != static_cast<uint32_t>(stride))
  {
    m_config.tangent.stride = static_cast<uint32_t>(stride);
    tangentStrideChanged(stride);
    configurationChanged();
  }
}

void Model::setTangentBuffer(int buf)
{
  if(m_config.tangent.buffer_index != buf)
  {
    m_config.tangent.buffer_index = buf;
    tangentBufferChanged(buf);
    configurationChanged();
  }
}

QString Model::prettyName() const noexcept
{
  return tr("Buffer Geometry");
}

}

template <>
void DataStreamReader::read(const Gfx::BufferGeometry::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::BufferGeometry::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::BufferGeometry::Model& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gfx::BufferGeometry::Model& proc)
{
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
