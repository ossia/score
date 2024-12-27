#include "PCLToGeometry.hpp"

#include <Threedim/MeshHelpers.hpp>
#include <Threedim/TinyObj.hpp>
#include <rnd/random.hpp>

#include <QDebug>
#include <QString>

namespace Threedim
{
void PCLToMesh::operator()()
{
  auto& tex = this->inputs.in.texture;
  if (!tex.changed)
    return;

  float* data = reinterpret_cast<float*>(tex.bytes);
  create_mesh(std::span<float>(data, tex.bytesize / sizeof(float)));
}
void PCLToMesh::create_mesh(std::span<float> v)
{
  {
    // std::size_t vertices = v.size() / 3;

    // this->complete.clear();
    // this->complete.resize(std::ceil((v.size() / 3.) * (3 + 3 + 2)));
    // std::copy_n(v.begin(), v.size(), complete.begin());

    // auto& pch = rnd::fast_random_device();
    //    this->complete.resize(6 * 25000);
    //    for (float& v : this->complete)
    //      v = std::uniform_real_distribution<>{0.f, 1.f}(pch);

    complete.assign(v.begin(), v.end());

    outputs.geometry.mesh.buffers.main_buffer.data = complete.data();
    outputs.geometry.mesh.buffers.main_buffer.size = complete.size();
    outputs.geometry.mesh.buffers.main_buffer.dirty = true;

    outputs.geometry.mesh.input.input0.offset = 0;
    outputs.geometry.mesh.vertices = v.size() / 6;
    outputs.geometry.dirty_mesh = true;
  }
}
}
