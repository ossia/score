#pragma once
#include <Gfx/Graph/FloatImageIO.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <functional>
#include <memory>
#include <string>

namespace Threedim
{

//! A calibration map file (PFM; `name.xyz.pfm` takes its fourth channel from
//! `name.valid.pfm`; 8 / 16-bit images through QImage) as an RGBA32F texture.
class LoadFloatTexture
{
public:
  halp_meta(name, "Load Float Texture")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "load_float_texture")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "Load a float map (PFM, rows stored bottom to top; a name.xyz.pfm picks up "
      "name.valid.pfm as its alpha), a 16-bit image, a Paul Bourke warp mesh (.data, as "
      "a UV map) or the geometry warp of an MPCDI file's first region as an RGBA32F "
      "texture.")
  halp_meta(uuid, "7b3e0f52-9c1d-4e86-a2f4-6d81c05e9b37")

  struct ins
  {
    struct map_t : halp::file_port<"File", halp::mmap_file_view>
    {
      halp_meta(
          extensions,
          "Float maps (*.pfm *.png *.tif *.tiff *.exr);;Warp files (*.data *.mpcdi)");
      static std::function<void(LoadFloatTexture&)> process(file_type data)
      {
        auto img = std::make_shared<score::gfx::FloatImage>(
            load(data.filename, data.bytes));
        return [img = std::move(img)](LoadFloatTexture& self) mutable {
          self.m_pending = std::move(img);
        };
      }
    } file;
  } inputs;

  struct
  {
    halp::texture_output<"Output", halp::rgba32f_texture> texture;
    halp::val_port<"Size", halp::xy_type<float>> size;
    halp::val_port<"Loaded", bool> loaded;
  } outputs;

  static score::gfx::FloatImage load(std::string_view filename, std::string_view bytes);

  void operator()();

private:
  std::shared_ptr<score::gfx::FloatImage> m_pending;
};

//! Reads a texture back and writes it as PFM (CONVENTIONS.md: rows bottom to
//! top; an RGBA map as `name.xyz.pfm` + `name.valid.pfm`) with an optional
//! `name.json` sidecar carrying the calibration it belongs to.
class SaveFloatTexture
{
public:
  halp_meta(name, "Save Float Texture")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "save_float_texture")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "On Save, read the input texture back and write it as PFM: RGBA as name.xyz.pfm "
      "+ name.valid.pfm, RGB or the red channel as name.pfm, with an optional name.json "
      "holding the intrinsics, size, pose, viewer and RMS.")
  halp_meta(uuid, "e2a94c71-0b5d-4f38-9d6e-41c7b8f03a26")

  enum class Channels
  {
    RGBA,
    RGB,
    Red
  };

  struct ins
  {
    halp::texture_input<"In", halp::rgba32f_texture> image;
    halp::save_file_path<"File"> file;
    halp::enum_t<Channels, "Channels"> channels;
    halp::impulse_button<"Save"> save;
    halp::toggle<"JSON sidecar"> sidecar;
    halp::val_port<"Intrinsics", std::array<float, 4>> intrinsics;
    halp::val_port<"World from projector", std::array<float, 16>> pose{
        .value = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    halp::val_port<"Viewer", halp::xyz_type<float>> viewer;
    halp::val_port<"RMS", float> rms;
  } inputs;

  struct
  {
    halp::val_port<"Written", std::string> written;
    halp::val_port<"Saved", bool> saved;
  } outputs;

  //! `dir/name`, `dir/name.pfm` or `dir/name.xyz.pfm` -> `dir/name`.
  static std::string basePath(std::string path);

  //! The image the texture readback (rows top to bottom) holds, in the chosen channels.
  static score::gfx::FloatImage
  extract(const float* rgba, int width, int height, Channels channels);

  void operator()();
};
}
