#pragma once

#include <QDebug>

#include <halp/geometry.hpp>
#include <magic_enum/magic_enum.hpp>

namespace halp
{
inline QDebug operator<<(QDebug dbg, const dynamic_geometry& geom)
{
  QDebugStateSaver saver(dbg);
  dbg.nospace();

  dbg << "dynamic_geometry {\n";

  // Basic properties
  dbg << "  vertices: " << geom.vertices << "\n";
  dbg << "  topology: " << magic_enum::enum_name(geom.topology) << "\n";
  dbg << "  cull_mode: " << magic_enum::enum_name(geom.cull_mode) << "\n";
  dbg << "  front_face: " << magic_enum::enum_name(geom.front_face) << "\n";

  // Buffers
  dbg << "  buffers[" << geom.buffers.size() << "]: {\n";
  for(std::size_t i = 0; i < geom.buffers.size(); ++i)
  {
    const auto& buf = geom.buffers[i];
    dbg << "    [" << i << "] { ralw_data: " << buf.raw_data
        << ", size: " << buf.byte_size << ", dirty: " << (buf.dirty ? "true" : "false")
        << " }\n";
  }
  dbg << "  }\n";

  // Bindings
  dbg << "  bindings[" << geom.bindings.size() << "]: {\n";
  for(std::size_t i = 0; i < geom.bindings.size(); ++i)
  {
    const auto& bind = geom.bindings[i];
    dbg << "    [" << i << "] { stride: " << bind.stride
        << ", step_rate: " << bind.step_rate
        << ", classification: " << magic_enum::enum_name(bind.classification) << " }\n";
  }
  dbg << "  }\n";

  // Attributes
  dbg << "  attributes[" << geom.attributes.size() << "]: {\n";
  for(std::size_t i = 0; i < geom.attributes.size(); ++i)
  {
    const auto& attr = geom.attributes[i];
    dbg << "    [" << i << "] { binding: " << attr.binding
        << ", location: " << magic_enum::enum_name(attr.location)
        << ", format: " << magic_enum::enum_name(attr.format)
        << ", offset: " << attr.byte_offset << " }\n";
  }
  dbg << "  }\n";

  // Inputs
  dbg << "  input[" << geom.input.size() << "]: {\n";
  for(std::size_t i = 0; i < geom.input.size(); ++i)
  {
    const auto& inp = geom.input[i];
    dbg << "    [" << i << "] { buffer: " << inp.buffer
        << ", offset: " << inp.byte_offset << " }\n";
  }
  dbg << "  }\n";

  // Index buffer
  dbg << "  index: { buffer: " << geom.index.buffer;
  if(geom.index.buffer >= 0)
  {
    dbg << ", offset: " << geom.index.byte_offset
        << ", format: " << magic_enum::enum_name(geom.index.format);
  }
  else
  {
    dbg << " (none)";
  }
  dbg << " }\n";

  dbg << "}";

  return dbg;
}
inline QDebug operator<<(QDebug dbg, const dynamic_gpu_geometry& geom)
{
  QDebugStateSaver saver(dbg);
  dbg.nospace();

  dbg << "dynamic_gpu_geometry {\n";

  // Basic properties
  dbg << "  vertices: " << geom.vertices << "\n";
  dbg << "  topology: " << magic_enum::enum_name(geom.topology) << "\n";
  dbg << "  cull_mode: " << magic_enum::enum_name(geom.cull_mode) << "\n";
  dbg << "  front_face: " << magic_enum::enum_name(geom.front_face) << "\n";

  // Buffers
  dbg << "  buffers[" << geom.buffers.size() << "]: {\n";
  for(std::size_t i = 0; i < geom.buffers.size(); ++i)
  {
    const auto& buf = geom.buffers[i];
    dbg << "    [" << i << "] { handle: " << buf.handle << ", size: " << buf.byte_size
        << ", dirty: " << (buf.dirty ? "true" : "false") << " }\n";
  }
  dbg << "  }\n";

  // Bindings
  dbg << "  bindings[" << geom.bindings.size() << "]: {\n";
  for(std::size_t i = 0; i < geom.bindings.size(); ++i)
  {
    const auto& bind = geom.bindings[i];
    dbg << "    [" << i << "] { stride: " << bind.stride
        << ", step_rate: " << bind.step_rate
        << ", classification: " << magic_enum::enum_name(bind.classification) << " }\n";
  }
  dbg << "  }\n";

  // Attributes
  dbg << "  attributes[" << geom.attributes.size() << "]: {\n";
  for(std::size_t i = 0; i < geom.attributes.size(); ++i)
  {
    const auto& attr = geom.attributes[i];
    dbg << "    [" << i << "] { binding: " << attr.binding
        << ", location: " << magic_enum::enum_name(attr.location)
        << ", format: " << magic_enum::enum_name(attr.format)
        << ", offset: " << attr.byte_offset << " }\n";
  }
  dbg << "  }\n";

  // Inputs
  dbg << "  input[" << geom.input.size() << "]: {\n";
  for(std::size_t i = 0; i < geom.input.size(); ++i)
  {
    const auto& inp = geom.input[i];
    dbg << "    [" << i << "] { buffer: " << inp.buffer
        << ", offset: " << inp.byte_offset << " }\n";
  }
  dbg << "  }\n";

  // Index buffer
  dbg << "  index: { buffer: " << geom.index.buffer;
  if(geom.index.buffer >= 0)
  {
    dbg << ", offset: " << geom.index.byte_offset
        << ", format: " << magic_enum::enum_name(geom.index.format);
  }
  else
  {
    dbg << " (none)";
  }
  dbg << " }\n";

  dbg << "}";

  return dbg;
}

}
