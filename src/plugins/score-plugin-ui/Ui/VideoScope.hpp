#pragma once
#include <Process/Monitoring/Monitor.hpp>

#include <QImage>
#include <QPainter>

#include <Ui/Monitor/ImageAnalysis.hpp>
#include <Ui/Monitor/LayerHelpers.hpp>
#include <Gfx/Widgets/OutletPreviewLayer.hpp>
#include <Ui/Monitor/Paint.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <atomic>
#include <memory>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiComputePipeline;
class QRhiResourceUpdateBatch;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
struct QRhiReadbackResult;
namespace score::gfx
{
class RenderList;
struct Edge;
}

/**
 * Video scopes computed on the GPU: waveform, parades, vectorscope,
 * histogram, CIE 1931 chromaticity and Tektronix-style diamond.
 *
 * Three compute passes per frame: the bins of the previous frame fade by the
 * persistence, every sampled input pixel adds to the bins of the mode, and
 * the bins are tone-mapped into the scope image, which is the texture output.
 * The labelled graticule is drawn on the CPU and composited on the GPU.
 *
 * The view shows that output through a preview of the outlet: being an output
 * of the graph, the preview pulls the scope, which renders whenever it is
 * shown, cabled further or not.
 */
namespace Ui::VideoScope
{
enum class Mode
{
  Waveform,
  RgbOverlay,
  RgbParade,
  YCbCrParade,
  Vectorscope,
  Histogram,
  CIE,
  Diamond
};
inline constexpr std::string_view mode_names[]{
    "Waveform", "RGB overlay", "RGB parade", "YCbCr parade",
    "Vectorscope", "Histogram", "CIE 1931", "Diamond"};

enum class Units
{
  Percent,
  Code8,
  Code10,
  Millivolts,
  Nits
};
inline constexpr std::string_view units_names[]{"%", "8-bit", "10-bit", "mV", "nits"};

//! Where scope values land in the scope image, in [0, 1] from the top left.
//! The shaders map their bins the same way.
namespace geometry
{
//! Waveforms show from -7% to 109%, so that sub-blacks and super-whites show
inline double waveY(double v) noexcept
{
  return 1. - (v + 0.07) / 1.16;
}
inline QPointF vector(double cb, double cr, double zoom) noexcept
{
  return {0.5 + cb * zoom, 0.5 - cr * zoom};
}
inline QPointF cie(double x, double y) noexcept
{
  return {x / 0.8, 1. - y / 0.9};
}
//! The upper diamond plots B'-G' against B'+G', the lower R'-G' against
//! -(R'+G')
inline QPointF diamondUpper(double b, double g) noexcept
{
  return {0.5 + (b - g) * 0.25, 0.5 - (b + g) * 0.25};
}
inline QPointF diamondLower(double r, double g) noexcept
{
  return {0.5 + (r - g) * 0.25, 0.5 + (r + g) * 0.25};
}
}

struct Layer;

//! What the views tell the scope, whichever list renders it: start over.
struct ScopeMonitor final : Process::Monitor
{
  std::atomic<uint32_t> clears{};
};

struct Node
{
  halp_meta(name, "Video scope")
  halp_meta(c_name, "VideoScope")
  halp_meta(category, "Monitoring/Visuals")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(
      description,
      "Waveform, parades, vectorscope, histogram, CIE chromaticity and diamond "
      "scopes of a texture, computed on the GPU; the scope is also a texture")
  halp_meta(uuid, "08283fcb-46ac-460c-b796-b3df1cb77d96")
  halp_flag(fully_custom_item);
  using Layer = Ui::VideoScope::Layer;

  //! The scope is drawn at the size it is shown at, so that its labels
  //! keep the size of the other text of the views; within these bounds.
  static constexpr int min_size = 128, max_size = 2048;

  struct
  {
    halp::gpu_texture_input<"In"> image;
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[8]{mode_names[0], mode_names[1], mode_names[2], mode_names[3],
                                   mode_names[4], mode_names[5], mode_names[6], mode_names[7]};
        Mode init{Mode::Waveform};
      };
    } mode;
    struct : halp::combobox_t<"Matrix", Ui::Monitor::Color::Matrix>
    {
      halp_meta(description, "Luma and colour difference coefficients, and the primaries of the CIE plot")
      struct range
      {
        std::string_view values[3]{"Rec. 601", "Rec. 709", "Rec. 2020"};
        Ui::Monitor::Color::Matrix init{Ui::Monitor::Color::Matrix::Rec709};
      };
    } matrix;
    struct : halp::combobox_t<"Transfer", Ui::Monitor::Color::Transfer>
    {
      halp_meta(description, "How the signal is encoded: for the CIE plot and the nits scale")
      struct range
      {
        std::string_view values[3]{"SDR", "PQ", "HLG"};
        Ui::Monitor::Color::Transfer init{Ui::Monitor::Color::Transfer::Sdr};
      };
    } transfer;
    struct : halp::combobox_t<"Units", Units>
    {
      struct range
      {
        std::string_view values[5]{
            units_names[0], units_names[1], units_names[2], units_names[3], units_names[4]};
        Units init{Units::Percent};
      };
    } units;
    struct : halp::toggle<"Legal range">
    {
      halp_meta(description, "Signal in narrow range: 0% at code 16 and 100% at 235 (8-bit)")
    } legal;
    struct : halp::hslider_f32<"Intensity", halp::range{0.1, 10., 1.}>
    {
    } intensity;
    struct : halp::hslider_f32<"Persistence", halp::range{0., 0.98, 0.5}>
    {
    } persistence;
    struct : halp::toggle_t<bool, "75% targets", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Vectorscope targets of 75% colour bars; off for 100% bars")
    } targets75;
    struct : halp::hslider_f32<"Zoom", halp::range{1., 5., 1.}>
    {
      halp_meta(description, "Vectorscope magnification")
    } zoom;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Scope"> scope;
  } outputs;

  std::shared_ptr<ScopeMonitor> monitor = std::make_shared<ScopeMonitor>();

  Node();
  ~Node();

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void runInitialPasses(
      score::gfx::RenderList& r, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
      score::gfx::Edge& e);
  void release(score::gfx::RenderList& r);
  void operator()() { }

private:
  bool createPipelines(score::gfx::RenderList& r);
  //! The bins, the scope and its graticule, at `sz` pixels square.
  void allocate(int sz);
  bool rebuildBindings(QRhiTexture* input);

  QRhi* m_rhi{};
  QRhiBuffer* m_params{};
  QRhiBuffer* m_bins{};
  int size{};
  QRhiTexture* m_out{};
  //! The graticule, drawn on the CPU and composited by the render pass
  QRhiTexture* m_overlay{};
  int m_overlayKey{-1};
  bool m_clearBins{};
  uint32_t m_clears{};
  QRhiSampler* m_sampler{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_decay{};
  QRhiComputePipeline* m_accumulate{};
  QRhiComputePipeline* m_render{};
  void* m_boundInput{};
  Ui::Monitor::Color::Transfer m_builtTransfer{};
  bool m_pipelinesFailed{};
};

/**
 * The scope image with its graticule and labels, drawn at the view's
 * resolution.
 */
void drawGraticule(
    QPainter& p, const QRectF& r, Mode m, Units u, bool legal,
    Ui::Monitor::Color::Matrix matrix, Ui::Monitor::Color::Transfer transfer,
    bool targets75, double zoom);

struct Layer : public Gfx::OutletPreviewLayer
{
  Layer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent);

  //! A double click clears what the scope has accumulated
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;

private:
  const Process::ProcessModel& m_scope;
  Ui::Monitor::DoubleClick m_doubleClick;
};
}
