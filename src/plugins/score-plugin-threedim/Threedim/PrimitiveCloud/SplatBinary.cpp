#include "SplatBinary.hpp"

#include <cstdint>
#include <cstring>
#include <memory>

namespace Threedim::PrimitiveCloud
{

ossia::primitive_cloud_component_ptr parse_splat_binary(std::string_view bytes)
{
  constexpr uint32_t kRowSize = 32;
  if(bytes.empty() || (bytes.size() % kRowSize) != 0)
    return nullptr;

  const uint32_t N = (uint32_t)(bytes.size() / kRowSize);
  if(N == 0)
    return nullptr;

  // Copy into a stable shared buffer. The input string_view points at
  // halp's mmap or text-file storage which doesn't outlive this call.
  auto storage = std::shared_ptr<uint8_t[]>(new uint8_t[bytes.size()]);
  std::memcpy(storage.get(), bytes.data(), bytes.size());

  // AABB from first 12 bytes of each row (xyz floats).
  ossia::aabb bounds{};
  bounds.min[0] = bounds.min[1] = bounds.min[2] = 1.f;
  bounds.max[0] = bounds.max[1] = bounds.max[2] = -1.f;
  for(uint32_t i = 0; i < N; ++i)
  {
    float x, y, z;
    std::memcpy(&x, storage.get() + i * kRowSize + 0,  sizeof(float));
    std::memcpy(&y, storage.get() + i * kRowSize + 4,  sizeof(float));
    std::memcpy(&z, storage.get() + i * kRowSize + 8,  sizeof(float));
    bounds.expand(x, y, z);
  }

  auto br = std::make_shared<ossia::buffer_resource>();
  br->resource = ossia::buffer_data{
      .data = std::shared_ptr<const void>(storage, storage.get()),
      .byte_size = (int64_t)bytes.size(),
      .usage_hint = ossia::buffer_data::usage::storage_buffer};
  br->content_hash = (uint64_t)(uintptr_t)storage.get();

  auto out = std::make_shared<ossia::primitive_cloud_component>();
  out->raw_data = std::move(br);
  out->row_stride = kRowSize;
  out->primitive_count = N;
  out->topology = ossia::primitive_topology::points;
  out->format_id = "3dgs.splat-binary";
  out->bounds = bounds;
  out->stable_id = ossia::mint_stable_id();
  return out;
}

} // namespace Threedim::PrimitiveCloud
