#pragma once
#include <Threedim/CalibrationJobs.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Threedim
{

class LedWallMap
{
public:
  halp_meta(name, "LED Wall Map")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "led_wall_map")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "The XYZ map (RGBA32F, w = 1 on LED pixels) of an LED surface in its output "
      "raster: a flat wall from its four corners and pixel size, or cabinets from a CSV "
      "file (one per line: x, y, w, h in the raster, then the top-left, top-right, "
      "bottom-left and bottom-right corners in metres) for curved or stepped walls, "
      "gaps left invalid. The same geometry an nDisplay mesh or an MPCDI 3D profile "
      "carries. Also gives the Kooima screen corners of the wall (Generalized "
      "Perspective Projection, 2008) for a Camera in Screen mode, and of each cabinet. "
      "Save writes it as a calibration (name.xyz.pfm, name.json).")
  halp_meta(uuid, "8d4f2b61-c7a3-4e19-b5d0-93e6a1f72c48")

  enum Mode
  {
    Flat,
    Cabinets
  };

  struct Corners
  {
    std::array<float, 3> tl{}, tr{}, bl{}, br{};
    bool operator==(const Corners&) const = default;
  };
  struct Settings
  {
    int mode{Flat};
    std::array<int, 2> size{};
    Corners corners;
    std::array<int, 2> raster{};
    uint64_t cabinets{};
    bool operator==(const Settings&) const = default;
  };

  struct Built
  {
    bool ok{};
    std::string status;
    int width{}, height{};
    std::vector<float> map;
    std::array<float, 3> pa{}, pb{}, pc{};
    std::vector<float> screens;
  };
  struct Saved
  {
    std::string status;
  };

  struct BuildJob
  {
    Settings settings;
    std::shared_ptr<const std::string> csv;
  };
  struct SaveJob
  {
    std::string path;
    std::shared_ptr<const Built> built;
  };
  using Job = std::variant<BuildJob, SaveJob>;

  struct Shared
  {
    CalibrationJobs<Built> builds;
    CalibrationJobs<Saved> saves;
  };

  struct ins
  {
    struct : halp::enum_t<Mode, "Mode">
    {
      struct range
      {
        std::string_view values[2]{"Flat wall", "Cabinets (CSV)"};
        Mode init{Flat};
      };
    } mode;
    struct Size : halp::xy_spinboxes_i32<"Pixels", halp::range{1, 16384, 1920}>
    {
      Size() { value = {1920, 1080}; }
    } size;
    struct TL : halp::xyz_spinboxes_f32<"Top left", halp::range{-10000., 10000., 0.}>
    {
      TL() { value = {-2.f, 2.25f, -3.f}; }
    } tl;
    struct TR : halp::xyz_spinboxes_f32<"Top right", halp::range{-10000., 10000., 0.}>
    {
      TR() { value = {2.f, 2.25f, -3.f}; }
    } tr;
    struct BL : halp::xyz_spinboxes_f32<"Bottom left", halp::range{-10000., 10000., 0.}>
    {
      BL() { value = {-2.f, 0.f, -3.f}; }
    } bl;
    struct BR : halp::xyz_spinboxes_f32<"Bottom right", halp::range{-10000., 10000., 0.}>
    {
      BR() { value = {2.f, 0.f, -3.f}; }
    } br;
    struct csv_t : halp::file_port<"Cabinet file", halp::text_file_view>
    {
      halp_meta(extensions, "Cabinets (*.csv *.txt)");
      static std::function<void(LedWallMap&)> process(file_type data);
    } csv;
    //! 0 = the bounding size of the cabinets.
    struct Raster : halp::xy_spinboxes_i32<"Raster size", halp::range{0, 16384, 0}>
    {
    } raster;
    halp::save_file_path<"Save file"> save_path;
    halp::impulse_button<"Save"> save;
    halp::impulse_button<"Update"> update;
  } inputs;

  struct
  {
    halp::texture_output<"XYZ map", halp::rgba32f_texture> xyz;
    halp::val_port<"Screen lower left", halp::xyz_type<float>> pa;
    halp::val_port<"Screen lower right", halp::xyz_type<float>> pb;
    halp::val_port<"Screen upper left", halp::xyz_type<float>> pc;
    //! Per cabinet: pa, pb, pc, 9 floats each.
    halp::val_port<"Cabinet screens", std::vector<float>> screens;
    halp::val_port<"Size", halp::xy_type<int>> out_size;
    halp::val_port<"Status", std::string> status;
  } outputs;

  struct worker
  {
    std::function<void(std::weak_ptr<Shared>, uint64_t, std::shared_ptr<const Job>)> request;
    static bool must_run(
        const std::weak_ptr<Shared>& s, uint64_t, const std::shared_ptr<const Job>&)
    {
      return !s.expired();
    }
    static void work(std::weak_ptr<Shared> s, uint64_t id, std::shared_ptr<const Job> job);
  } worker;

  std::shared_ptr<Shared> node_state;

  //! The map of `s`; `csv` is the cabinet file of Cabinets mode.
  static Built build(const Settings& s, const std::string& csv);

  LedWallMap() = default;
  LedWallMap(const LedWallMap&) = delete;
  LedWallMap& operator=(const LedWallMap&) = delete;
  ~LedWallMap();

  void operator()();

  //! The last map built.
  std::shared_ptr<const Built> built() const noexcept { return m_built; }

private:
  Shared& shared();
  Settings settings() const;
  std::shared_ptr<const std::string> m_csv = std::make_shared<const std::string>();
  uint64_t m_csvVersion{};
  std::optional<Settings> m_requested;
  std::shared_ptr<const Built> m_built;
  uint64_t m_builtSeen{}, m_savedSeen{};
};
}
