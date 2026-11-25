#pragma once
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Threedim/CalibrationJobs.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <QByteArray>
#include <QString>

#include <array>
#include <functional>
#include <memory>
#include <string>

namespace Threedim
{
//! An RGBA32F input read back on request, at its producer's size.
template <halp::static_string lit>
struct FloatMapInput : halp::texture_input<lit, halp::rgba32f_texture>
{
  bool request_readback{};
  static constexpr bool source_size() { return true; }
  bool resampled{};
};

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

  struct Loaded
  {
    score::gfx::FloatImage image;
    std::string status;
  };
  struct Job
  {
    QString filename;
    QByteArray bytes;
  };
  using Jobs = CalibrationJobs<Loaded>;

  struct ins
  {
    struct map_t : halp::file_port<"File", halp::mmap_file_view>
    {
      halp_meta(
          extensions,
          "Float maps (*.pfm *.png *.tif *.tiff *.exr);;Warp files (*.data *.mpcdi)");
      static std::function<void(LoadFloatTexture&)> process(file_type data);
    } file;
  } inputs;

  struct
  {
    halp::texture_output<"Output", halp::rgba32f_texture> texture;
    halp::val_port<"Size", halp::xy_type<float>> size;
    halp::val_port<"Loaded", bool> loaded;
    halp::val_port<"Status", std::string> status;
  } outputs;

  struct worker
  {
    std::function<void(std::weak_ptr<Jobs>, uint64_t, std::shared_ptr<const Job>)> request;
    static bool must_run(const std::weak_ptr<Jobs>& jobs, uint64_t, const std::shared_ptr<const Job>&)
    {
      return !jobs.expired();
    }
    static void work(std::weak_ptr<Jobs> jobs, uint64_t id, std::shared_ptr<const Job> job);
  } worker;

  std::shared_ptr<Jobs> node_state;

  //! A map from its file name and contents; the name only says what the
  //! contents are, and where an MPCDI archive or a valid.pfm next to it is.
  static Loaded load(const QString& filename, const QByteArray& bytes);

  LoadFloatTexture() = default;
  LoadFloatTexture(const LoadFloatTexture&) = delete;
  LoadFloatTexture& operator=(const LoadFloatTexture&) = delete;
  ~LoadFloatTexture();

  void operator()();

private:
  Jobs& jobs();
  void request(Job job);
  uint64_t m_seen{};
};

//! Reads a texture back and writes it as PFM (rows bottom to top; an RGBA
//! map as `name.xyz.pfm` + `name.valid.pfm`), with the calibration
//! it belongs to in `name.json` (writeCalibration) when JSON sidecar is on.
class SaveFloatTexture
{
public:
  halp_meta(name, "Save Float Texture")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "save_float_texture")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "On Save, read the input texture back at its own size and write it as PFM: RGBA "
      "as name.xyz.pfm + name.valid.pfm, RGB or the red channel as name.pfm. With JSON "
      "sidecar, name.json lists the files and holds the size and, when the intrinsics "
      "are given, the intrinsics, pose, viewer and RMS; the other keys of an existing "
      "name.json are kept. Without it, a name.json listing the files is not written "
      "over.")
  halp_meta(uuid, "e2a94c71-0b5d-4f38-9d6e-41c7b8f03a26")

  enum class Channels
  {
    RGBA,
    RGB,
    Red
  };

  struct ins
  {
    FloatMapInput<"In"> image;
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
    halp::val_port<"Status", std::string> status;
  } outputs;

  struct Job
  {
    QString path;
    score::gfx::FloatImage image;
    bool sidecar{};
    std::array<float, 4> intrinsics{};
    std::array<float, 16> pose{};
    std::array<float, 3> viewer{};
    float rms{};
  };
  struct Result
  {
    bool ok{};
    std::string written;
    std::string status;
  };
  using Jobs = CalibrationJobs<Result>;

  struct worker
  {
    std::function<void(std::weak_ptr<Jobs>, uint64_t, std::shared_ptr<const Job>)> request;
    static bool must_run(const std::weak_ptr<Jobs>&, uint64_t, const std::shared_ptr<const Job>&) { return true; }
    static void work(std::weak_ptr<Jobs> jobs, uint64_t id, std::shared_ptr<const Job> job);
  } worker;

  //! The image the texture readback (rows top to bottom) holds, in the chosen channels.
  static score::gfx::FloatImage
  extract(const float* rgba, int width, int height, Channels channels);

  static Result write(const Job& job);

  //! Frames a Save waits for its readback.
  static constexpr int readbackFrames = 120;

  void operator()();

private:
  std::shared_ptr<Jobs> m_jobs = std::make_shared<Jobs>();
  uint64_t m_seen{};
  int m_waited{-1};
};
}
