#include "LedWall.hpp"

#include <Gfx/Calibration/LedMapping.hpp>
#include <Gfx/Window/CalibrationSet.hpp>
#include <Threedim/CalibrationFiles.hpp>

#include <cstring>

namespace Threedim
{
namespace led = score::gfx::led;

LedWallMap::Built LedWallMap::build(const Settings& s, const std::string& csv)
{
  Built b;
  auto v3 = [](const std::array<float, 3>& p) { return led::Vec3(p[0], p[1], p[2]); };
  std::vector<led::Module> modules;
  std::string err;
  if(s.mode == Flat)
  {
    led::Module m;
    m.w = std::max(1, s.size[0]);
    m.h = std::max(1, s.size[1]);
    m.tl = v3(s.corners.tl);
    m.tr = v3(s.corners.tr);
    m.bl = v3(s.corners.bl);
    m.br = v3(s.corners.br);
    modules.push_back(m);
  }
  else
  {
    if(csv.empty())
    {
      b.status = "Failed: no cabinet file";
      return b;
    }
    modules = led::parseModules(csv, &err);
    if(modules.empty())
    {
      b.status = "Failed: no cabinet in the cabinet file" + (err.empty() ? "" : " (" + err + ")");
      return b;
    }
  }
  auto sz = led::rasterSize(modules);
  if(s.mode == Cabinets && s.raster[0] > 0 && s.raster[1] > 0)
    sz = {s.raster[0], s.raster[1]};
  b.width = sz[0];
  b.height = sz[1];
  b.map = led::rasterize(modules, b.width, b.height);
  if(b.map.empty())
  {
    b.status = "Failed: a " + std::to_string(b.width) + " x " + std::to_string(b.height)
               + " raster is too large";
    b.width = b.height = 0;
    return b;
  }

  const auto bs = led::boundingScreen(modules);
  auto out3 = [](const led::Vec3& p) {
    return std::array<float, 3>{float(p.x()), float(p.y()), float(p.z())};
  };
  b.pa = out3(bs.pa);
  b.pb = out3(bs.pb);
  b.pc = out3(bs.pc);
  for(const auto& m : modules)
  {
    const auto ms = led::moduleScreen(m);
    for(const auto* p : {&ms.pa, &ms.pb, &ms.pc})
      for(int c = 0; c < 3; c++)
        b.screens.push_back(float((*p)[c]));
  }
  std::size_t lit = 0;
  for(std::size_t i = 3; i < b.map.size(); i += 4)
    lit += b.map[i] > 0.f;
  b.status = "Done: " + std::to_string(modules.size()) + " cabinet(s), "
             + std::to_string(b.width) + " x " + std::to_string(b.height) + ", "
             + std::to_string(lit) + " LED pixels" + (err.empty() ? "" : "; " + err);
  b.ok = true;
  return b;
}

void LedWallMap::worker::work(
    std::weak_ptr<Shared> shared, uint64_t id, std::shared_ptr<const Job> job)
{
  std::visit(
      [&]<typename J>(const J& j) {
    if constexpr(std::is_same_v<J, BuildJob>)
    {
      auto b = build(j.settings, *j.csv);
      if(auto s = shared.lock())
        s->builds.finish(id, std::move(b));
    }
    else
    {
      CalibrationFile c;
      c.base = Gfx::calibrationBase(QString::fromStdString(j.path));
      c.width = j.built->width;
      c.height = j.built->height;
      c.xyz = j.built->map;
      c.json["mode"] = "led";
      QString err;
      Saved r;
      r.status = saveCalibration(c, &err)
                     ? "Saved " + QString(c.base + ".xyz.pfm").toStdString()
                     : "Failed: " + err.toStdString();
      if(auto s = shared.lock())
        s->saves.finish(id, std::move(r));
    }
  },
      *job);
}

std::function<void(LedWallMap&)> LedWallMap::ins::csv_t::process(file_type data)
{
  auto text = std::make_shared<const std::string>(data.bytes);
  return [text = std::move(text)](LedWallMap& self) mutable {
    self.m_csv = std::move(text);
    self.m_csvVersion++;
  };
}

LedWallMap::~LedWallMap()
{
  if(node_state)
    node_state->builds.release(this);
}

LedWallMap::Shared& LedWallMap::shared()
{
  if(!node_state)
    node_state = std::make_shared<Shared>();
  return *node_state;
}

LedWallMap::Settings LedWallMap::settings() const
{
  auto v3 = [](const auto& p) { return std::array<float, 3>{p.x, p.y, p.z}; };
  Settings s;
  s.mode = inputs.mode.value;
  s.size = {inputs.size.value.x, inputs.size.value.y};
  s.corners = {v3(inputs.tl.value), v3(inputs.tr.value), v3(inputs.bl.value), v3(inputs.br.value)};
  s.raster = {inputs.raster.value.x, inputs.raster.value.y};
  s.cabinets = m_csvVersion;
  return s;
}

void LedWallMap::operator()()
{
  auto& sh = shared();
  if(sh.builds.claim(this))
  {
    const auto s = settings();
    if((!m_requested || !(*m_requested == s) || inputs.update) && !sh.builds.busy())
    {
      m_requested = s;
      worker.request(
          node_state, sh.builds.start(),
          std::make_shared<const Job>(BuildJob{s, m_csv}));
    }
    if(inputs.save)
    {
      if(!m_built || !m_built->ok)
        outputs.status.value = "Failed: no map to save";
      else if(inputs.save_path.value.empty())
        outputs.status.value = "Failed: no Save file";
      else
        worker.request(
            node_state, sh.saves.start(),
            std::make_shared<const Job>(SaveJob{inputs.save_path.value, m_built}));
    }
  }

  if(auto b = sh.builds.fresh(m_builtSeen))
  {
    outputs.status.value = b->status;
    if(b->ok)
    {
      m_built = b;
      outputs.xyz.create(b->width, b->height);
      std::memcpy(outputs.xyz.texture.bytes, b->map.data(), b->map.size() * sizeof(float));
      outputs.xyz.upload();
      outputs.pa.value = {b->pa[0], b->pa[1], b->pa[2]};
      outputs.pb.value = {b->pb[0], b->pb[1], b->pb[2]};
      outputs.pc.value = {b->pc[0], b->pc[1], b->pc[2]};
      outputs.screens.value = b->screens;
      outputs.out_size.value = {b->width, b->height};
    }
  }
  if(auto r = sh.saves.fresh(m_savedSeen))
    outputs.status.value = r->status;
}
}
