#include "LedWall.hpp"

#include <Gfx/Calibration/LedMapping.hpp>
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/FloatTextureIO.hpp>

#include <QFile>

#include <cstring>

namespace Threedim
{
namespace led = score::gfx::led;

bool LedWallMap::build()
{
  auto v3 = [](const auto& p) { return led::Vec3(p.value.x, p.value.y, p.value.z); };
  std::vector<led::Module> modules;
  std::string err;
  if(inputs.mode.value == Flat)
  {
    led::Module m;
    m.w = std::max(1, inputs.size.value.x);
    m.h = std::max(1, inputs.size.value.y);
    m.tl = v3(inputs.tl);
    m.tr = v3(inputs.tr);
    m.bl = v3(inputs.bl);
    m.br = v3(inputs.br);
    modules.push_back(m);
  }
  else
  {
    QFile f{QString::fromStdString(inputs.csv.value)};
    if(!f.open(QIODevice::ReadOnly))
    {
      outputs.status.value = "Failed: cannot read " + inputs.csv.value;
      return false;
    }
    const QByteArray text = f.readAll();
    modules = led::parseModules(std::string_view(text.constData(), text.size()), &err);
    if(modules.empty())
    {
      outputs.status.value = "Failed: no cabinet in " + inputs.csv.value
                             + (err.empty() ? "" : " (" + err + ")");
      return false;
    }
  }
  auto sz = led::rasterSize(modules);
  if(inputs.mode.value == Cabinets && inputs.raster.value.x > 0 && inputs.raster.value.y > 0)
    sz = {inputs.raster.value.x, inputs.raster.value.y};
  width = sz[0];
  height = sz[1];
  map = led::rasterize(modules, width, height);

  const auto s = led::boundingScreen(modules);
  auto out3 = [](const led::Vec3& p) {
    return halp::xyz_type<float>{float(p.x()), float(p.y()), float(p.z())};
  };
  outputs.pa.value = out3(s.pa);
  outputs.pb.value = out3(s.pb);
  outputs.pc.value = out3(s.pc);
  outputs.screens.value.clear();
  for(const auto& m : modules)
  {
    const auto ms = led::moduleScreen(m);
    for(const auto* p : {&ms.pa, &ms.pb, &ms.pc})
      for(int c = 0; c < 3; c++)
        outputs.screens.value.push_back(float((*p)[c]));
  }
  outputs.positions.value.assign(map.begin(), map.end());
  outputs.out_size.value = {width, height};
  std::size_t lit = 0;
  for(std::size_t i = 3; i < map.size(); i += 4)
    lit += map[i] > 0.f;
  outputs.status.value = "Done: " + std::to_string(modules.size()) + " cabinet(s), "
                         + std::to_string(width) + " x " + std::to_string(height) + ", "
                         + std::to_string(lit) + " LED pixels"
                         + (err.empty() ? "" : "; " + err);
  return true;
}

void LedWallMap::operator()()
{
  std::string key;
  {
    auto add = [&](float v) { key.append(reinterpret_cast<const char*>(&v), sizeof(v)); };
    add(float(inputs.mode.value));
    add(float(inputs.size.value.x));
    add(float(inputs.size.value.y));
    for(auto* p : {&inputs.tl.value, &inputs.tr.value, &inputs.bl.value, &inputs.br.value})
    {
      add(p->x);
      add(p->y);
      add(p->z);
    }
    add(float(inputs.raster.value.x));
    add(float(inputs.raster.value.y));
    key += inputs.csv.value;
  }
  if(key != m_key || inputs.update)
  {
    m_key = key;
    if(build())
    {
      outputs.xyz.create(width, height);
      std::memcpy(outputs.xyz.texture.bytes, map.data(), map.size() * sizeof(float));
      outputs.xyz.upload();
    }
  }
  if(inputs.save && !map.empty())
  {
    CalibrationFile c;
    c.base = SaveFloatTexture::basePath(inputs.save_path.value);
    c.width = width;
    c.height = height;
    c.xyz = map;
    outputs.status.value = !c.base.empty() && saveCalibration(c)
                               ? "Saved " + c.base + ".xyz.pfm"
                               : "Failed: cannot save to " + inputs.save_path.value;
  }
}
}
