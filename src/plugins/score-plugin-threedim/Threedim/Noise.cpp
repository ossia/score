#include "Noise.hpp"

#include <PerlinNoise.hpp>

#include <QDebug>

#include <cassert>
namespace Threedim
{
Noise::Noise() { }

Noise::~Noise()
{
  if (!outputs.geometry.mesh.buffers.empty())
  {
    auto& b = outputs.geometry.mesh.buffers[0];
    delete[](float*)b.raw_data;
  }
}

static const siv::BasicPerlinNoise<double> engine{4u}; // chosen by fair dice roll
void Noise::operator()()
{
  auto old_bufs = outputs.geometry.mesh.buffers;
  (GeometryPort&)outputs.geometry = (const GeometryPort&)inputs.geometry;

  if (!outputs.geometry.mesh.buffers.empty())
  {
    int old_buf_idx = 0;
    for (auto& buf : outputs.geometry.mesh.buffers)
    {
      if(old_buf_idx < std::ssize(old_bufs))
      {
        auto old = old_bufs[old_buf_idx];
        auto cur = (float*)buf.raw_data;
        void* newb{};
        if(buf.byte_size == old.byte_size)
        {
          newb = old.raw_data;
        }
        else
        {
          delete[](float*)old.raw_data;
          assert(buf.byte_size / sizeof(float) >= outputs.geometry.mesh.vertices * 3);
          newb = new float[buf.byte_size / sizeof(float)];
        }
        memcpy(newb, cur, buf.byte_size);
        buf.raw_data = newb;
      }
      else
      {
        auto cur = (float*)buf.raw_data;
        buf.raw_data = new float[buf.byte_size / sizeof(float)];
        memcpy(buf.raw_data, cur, buf.byte_size);
      }
      old_buf_idx++;
    }
  }
  else
  {
    for (auto buf : old_bufs)
    {
      delete(float*)buf.raw_data;
    }
  }
  outputs.geometry.dirty_mesh = true;
  outputs.geometry.dirty_transform = true;

  //outputs.geometry.dirty = true;
  auto& mesh = outputs.geometry.mesh;
  if (mesh.buffers.empty())
    return;

  // Find the position attribute:
  auto& attr = mesh.attributes;
  auto it
      = std::find_if(attr.begin(), attr.end(), [](auto& a) { return a.location == halp::attribute_location::position; });
  if (it == attr.end())
    return;

  const int binding = it->binding;
  auto& ins = mesh.input;
  assert(binding >= 0);
  assert(binding < std::ssize(ins));

  const int buffer = ins[binding].buffer;

  auto& bufs = mesh.buffers;
  assert(buffer >= 0);
  assert(buffer < std::ssize(bufs));

  auto& buf = bufs[buffer];

  // FIXME Cheat a bit for now... and assume that position comes first,
  // and that things aren't interleaved,
  // and that we have float[3]s, ...
  // With mdspan we should be able to actually do these transformations
  // with simple code
  using type = float[3];
  std::span<type> vertices((type*)buf.raw_data, mesh.vertices);
  assert(buf.byte_size >= mesh.vertices * 3 * sizeof(float));

  using f_type = double (*)(double x, double intens, double t) noexcept;
  auto func = [&](DeformationControl::enum_type c) noexcept -> f_type
  {
    switch (c)
    {
      default:
      case DeformationControl::None:
        return [](double x, double intens, double t) noexcept { return x; };
        break;
      case DeformationControl::Noise:
        return [](double x, double intens, double t) noexcept {
          return (1. - intens) * x + intens * engine.noise1D(x + t);
        };
        break;
      case DeformationControl::Sine:
        return [](double x, double intens, double t) noexcept {
          return (1. - intens) * x + intens * std::sin(x + t);
        };
        break;
    }
  };

  const f_type fs[3] = {func(inputs.dx), func(inputs.dy), func(inputs.dz)};
  // FIXME proper tick support in CpuFilter / CpuAnalysis
  const double t = position_in_frames++ / 60.;

  const double ix = inputs.ix.value;
  const double iy = inputs.iy.value;
  const double iz = inputs.iz.value;

  for (float(&v)[3] : vertices)
  {
    v[0] = fs[0](v[0], ix, t + v[1] + v[2]);
    v[1] = fs[1](v[1], iy, t + v[0] + v[2]);
    v[2] = fs[2](v[2], iz, t + v[0] + v[1]);
  }
  mesh.buffers[0].dirty = true;
}

}
