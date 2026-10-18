#include "GeometryTopology.hpp"

#include <cstring>
#include <memory>

namespace score::gfx
{
std::vector<uint32_t> triangleFanToList(std::span<const uint32_t> order)
{
  std::vector<uint32_t> list;
  if(order.size() < 3)
    return list;
  list.reserve(3 * (order.size() - 2));
  for(std::size_t i = 2; i < order.size(); i++)
    list.insert(list.end(), {order[0], order[i - 1], order[i]});
  return list;
}

bool lowerTriangleFan(ossia::geometry& g)
{
  if(g.topology != ossia::geometry::triangle_fan)
    return false;
  if(!g.cpu_draw_commands.empty() || g.indirect_count.handle
     || g.find_auxiliary("_indirect_draw") || g.find_auxiliary("_indirect_draw_indexed"))
    return false;

  std::vector<uint32_t> order;
  if(g.index.buffer < 0)
  {
    order.resize(std::max(0, g.vertices));
    for(std::size_t i = 0; i < order.size(); i++)
      order[i] = uint32_t(i);
  }
  else
  {
    if(g.index.buffer >= (int)g.buffers.size())
      return false;
    const auto* cpu
        = ossia::get_if<ossia::geometry::cpu_buffer>(&g.buffers[g.index.buffer].data);
    if(!cpu || !cpu->raw_data)
      return false;
    const bool u16 = g.index.format == decltype(g.index)::uint16;
    const int64_t stride = u16 ? 2 : 4;
    const int64_t avail
        = std::max<int64_t>(0, (cpu->byte_size - g.index.byte_offset) / stride);
    const int64_t count = std::min<int64_t>(std::max(0, g.indices), avail);
    const auto* base = static_cast<const char*>(cpu->raw_data.get()) + g.index.byte_offset;
    order.resize(count);
    for(int64_t i = 0; i < count; i++)
    {
      if(u16)
      {
        uint16_t v;
        std::memcpy(&v, base + 2 * i, 2);
        order[i] = v;
      }
      else
      {
        std::memcpy(&order[i], base + 4 * i, 4);
      }
    }
  }

  const auto list = triangleFanToList(order);
  // An empty list still draws nothing, as the fan did; a zero-byte buffer
  // would be skipped as "not ready" by the mesh upload.
  const std::size_t bytes = std::max<std::size_t>(list.size(), 1) * sizeof(uint32_t);
  auto data = std::shared_ptr<uint32_t[]>(new uint32_t[bytes / sizeof(uint32_t)]{});
  std::memcpy(data.get(), list.data(), list.size() * sizeof(uint32_t));

  g.buffers.push_back(
      {.data = ossia::geometry::cpu_buffer{
           std::shared_ptr<void>(data, static_cast<void*>(data.get())), (int64_t)bytes},
       .dirty = true});

  g.index.buffer = int(g.buffers.size() - 1);
  g.index.byte_offset = 0;
  g.index.format = decltype(g.index)::uint32;
  g.indices = int(list.size());
  g.topology = ossia::geometry::triangles;
  return true;
}
}
