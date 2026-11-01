#pragma once
#include <QGraphicsSceneHoverEvent>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Gfx/Widgets/OutletPreviewLayer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <optional>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiComputePipeline;
class QRhiResourceUpdateBatch;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
namespace score::gfx
{
class RenderList;
struct Edge;
}

/**
 * Shows a texture, and what the eye cannot read from it: channels alone,
 * luma, exposure through false colour, clipping zebras, focus peaking,
 * values out of gamut and NaN or infinite values, or a comparison against a
 * reference by wipe or difference.
 *
 * One compute pass writes the inspected image, which is the texture output
 * and what the node shows. False colour, zebra and gamut follow the Video
 * Monitor shader of the library.
 */
namespace Ui::TextureView
{
enum class Mode
{
  View,
  Red,
  Green,
  Blue,
  Alpha,
  Luma,
  FalseColor,
  Zebra,
  Peaking,
  Gamut,
  Wipe,
  Difference
};
inline constexpr std::string_view mode_names[]{
    "View", "Red", "Green", "Blue", "Alpha", "Luma", "False colour", "Zebra",
    "Focus peaking", "Out of gamut", "Wipe", "Difference"};

enum class Background
{
  Checkerboard,
  Black,
  Grey,
  White,
  Magenta,
  IgnoreAlpha
};

enum class LumaStandard
{
  Rec709,
  Rec2020
};

struct Layer;

struct Node
{
  halp_meta(name, "Texture view")
  halp_meta(c_name, "TextureView")
  halp_meta(category, "Monitoring/Visuals")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(
      description,
      "Show a texture: channels, luma, false colour, zebras, focus peaking, "
      "out of gamut and NaN values, or a wipe or difference with a reference")
  halp_meta(uuid, "ede620d7-9f3f-4a0a-a6f0-27297cfe7801")
  halp_flag(fully_custom_item);
  using Layer = Ui::TextureView::Layer;

  struct
  {
    halp::gpu_texture_input<"In"> image;
    halp::gpu_texture_input<"Reference"> reference;
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[12]{
            mode_names[0], mode_names[1], mode_names[2], mode_names[3],
            mode_names[4], mode_names[5], mode_names[6], mode_names[7],
            mode_names[8], mode_names[9], mode_names[10], mode_names[11]};
        Mode init{Mode::View};
      };
    } mode;
    struct : halp::spinbox_f32<"Exposure", halp::range{-10., 10., 0.}>
    {
      halp_meta(description, "Stops of gain applied before display, to read floating point images")
    } exposure;
    struct : halp::toggle_t<bool, "NaN", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Flash pixels that are NaN or infinite, in every mode")
    } nan;
    struct : halp::combobox_t<"Background", Background>
    {
      halp_meta(description, "What transparent pixels are shown over in View mode")
      struct range
      {
        std::string_view values[6]{"Checkerboard", "Black", "Grey", "White", "Magenta", "Ignore alpha"};
        Background init{Background::Checkerboard};
      };
    } background;
    struct : halp::combobox_t<"Luma", LumaStandard>
    {
      struct range
      {
        std::string_view values[2]{"Rec. 709", "Rec. 2020"};
        LumaStandard init{LumaStandard::Rec709};
      };
    } luma;
    struct : halp::hslider_f32<"Zebra high", halp::range{0.5, 1.5, 0.95}>
    {
    } zebra_high;
    struct : halp::hslider_f32<"Zebra low", halp::range{0., 0.1, 0.02}>
    {
    } zebra_low;
    struct : halp::hslider_f32<"Peaking", halp::range{0.01, 1., 0.15}>
    {
      halp_meta(description, "Edge strength above which focus peaking marks a pixel")
    } peaking;
    struct : halp::hslider_f32<"Blend", halp::range{0., 1., 0.6}>
    {
      halp_meta(description, "How much the overlays cover the image")
    } blend;
    struct : halp::hslider_f32<"Wipe", halp::range{0., 1., 0.5}>
    {
      halp_meta(description, "Where the reference starts, from the left, in Wipe mode")
    } wipe;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Out"> texture;
    struct : halp::val_port<"Resolution", std::array<float, 2>>
    {
      halp_meta(description, "Width and height the input is rendered at, in pixels: the graph renders an input at the size its consumer shows it")
      enum widget
      {
        control
      };
    } resolution;
  } outputs;

  Node();
  ~Node();

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void runInitialPasses(
      score::gfx::RenderList& r, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
      score::gfx::Edge& e);
  void release(score::gfx::RenderList& r);
  void operator()() { }

private:
  void setBindings(QRhiTexture* input, QRhiTexture* reference);
  bool resizeOutput(QSize sz);

  QRhi* m_rhi{};
  QRhiBuffer* m_params{};
  QRhiTexture* m_out{};
  QRhiSampler* m_sampler{};
  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_pipeline{};
  void* m_boundInput{};
  void* m_boundReference{};
  int m_frame{};
};

struct Layer : public Gfx::OutletPreviewLayer
{
  Layer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent);

protected:
  void hoverMoveEvent(QGraphicsSceneHoverEvent* ev) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* ev) override;
  void paintOverlay(QPainter* p, const QRectF& image) const override;

private:
  std::array<float, 2> resolution() const;
  const Process::ControlOutlet* m_resolution{};
  std::optional<QPointF> m_hover;
};
}
