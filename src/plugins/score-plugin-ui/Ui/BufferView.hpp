#pragma once
#include <Process/Monitoring/Monitor.hpp>
#include <Process/Monitoring/MonitorLink.hpp>

#include <Effect/EffectLayer.hpp>

#include <QGraphicsSceneHoverEvent>
#include <QImage>
#include <QPolygonF>

#include <Ui/Monitor/BufferDecode.hpp>
#include <Ui/Monitor/BufferGather.hpp>
#include <Ui/Monitor/LayerHelpers.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

class QRhiResourceUpdateBatch;
namespace score::gfx
{
class RenderList;
struct Port;
}

/**
 * Shows what a GPU buffer holds: its elements as curves, a table, an image
 * or histograms. The bytes say nothing of what they are: the element type,
 * the components, the stride and the offset are the user's.
 *
 * While a view shows it, the node reads the buffer back at most at the rate
 * the views refresh at, and decodes it into floats on the render thread.
 * A compute pass first gathers the elements read into a buffer of their own,
 * so that a readback holds them and nothing between them, at most 16 MiB;
 * without compute shaders or for a buffer that is not a storage buffer, the
 * range from the first to the last element is read, cut to 16 MiB.
 */
namespace Ui::BufferView
{
using Ui::Monitor::Buffer::ElementType;
using Ui::Monitor::Buffer::Layout;
using Ui::Monitor::Buffer::Fallback;

enum class Mode
{
  Curves,
  Table,
  Image,
  Histogram
};
inline constexpr std::string_view mode_names[]{"Curves", "Table", "Image", "Histogram"};
inline constexpr std::string_view type_names[]{
    "Float (32 bit)", "Half (16 bit)",     "Int (32 bit)",
    "UInt (32 bit)",  "UInt8 (normalised)", "UInt8 (raw)"};

//! The elements of one readback, decoded.
struct Frame
{
  //! The components of each element, one after the other
  std::vector<float> values;
  //! Elements in `values`
  int64_t count{};
  //! Elements the buffer holds under the layout, read back or not
  int64_t available{};
  //! Index of the element `values` starts with
  int64_t first{};
  //! Elements of the buffer from one in `values` to the next
  int64_t step{1};
  //! Size of the buffer; 0 without one
  int64_t buffer_bytes{};
  //! Bytes the readback held
  int64_t read_bytes{};
  Layout layout{};
  //! Why the elements were read as one contiguous range
  Fallback fallback{};
  //! Whether the 16 MiB a readback holds kept `count` below what was asked
  bool capped{};
  //! Whether `range` holds the extent of every element from `first` to the
  //! end, not only of those in `values`
  bool exact{};
  Ui::Monitor::Buffer::GatherRange range{};
};

struct BufferMonitor final : Process::Monitor
{
  Process::LatestValue<Frame> frames;
  //! Milliseconds between two readbacks: the refresh interval of the views
  std::atomic<int> interval{16};
};

struct Layer;

struct Node
{
  halp_meta(name, "Buffer view")
  halp_meta(c_name, "BufferView")
  halp_meta(category, "Monitoring/Visuals")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(
      description,
      "Show the contents of a GPU buffer as curves, a table, an image or "
      "histograms, read with the element type, components, stride and offset given")
  halp_meta(uuid, "4c1f7a2e-93b8-4d56-a0e1-6b2d8f3c9e14")
  halp_flag(fully_custom_item);
  using Layer = Ui::BufferView::Layer;

  struct
  {
    struct : halp::gpu_buffer_input<"Buffer">
    {
      halp_flag(single_cable);
    } buffer;
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[4]{mode_names[0], mode_names[1], mode_names[2], mode_names[3]};
        Mode init{Mode::Curves};
      };
    } mode;
    struct : halp::combobox_t<"Type", ElementType>
    {
      halp_meta(description, "What each component of an element is")
      struct range
      {
        std::string_view values[6]{type_names[0], type_names[1], type_names[2],
                                   type_names[3], type_names[4], type_names[5]};
        ElementType init{ElementType::Float};
      };
    } type;
    struct : halp::spinbox_i32<"Components", halp::range{1, 4, 1}>
    {
      halp_meta(description, "Components of an element: 4 for a vec4")
    } components;
    struct : halp::spinbox_i32<"Stride", halp::range{0, 65536, 0}>
    {
      halp_meta(description, "Bytes from one element to the next; 0 when they follow each other")
    } stride;
    struct : halp::spinbox_i32<"Offset", halp::range{0, 2147483647, 0}>
    {
      halp_meta(description, "Bytes before element 0")
    } offset;
    struct : halp::spinbox_i32<"Width", halp::range{0, 16384, 0}>
    {
      halp_meta(description, "Elements per row in Image mode; 0 makes the image square")
    } width;
    struct : halp::toggle_t<bool, "Autoscale", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Fit the scales to the values; off, values are shown from 0 to 1")
    } autoscale;
    struct : halp::spinbox_i32<"Max elements", halp::range{1, 1048576, 65536}>
    {
      halp_meta(
          description,
          "Elements read back at most, from the first shown; a readback holds 16 MiB at most")
    } max_elements;
    // An int control: past 2^31 elements, Offset moves element 0 further
    struct : halp::spinbox_i32<"First element", halp::range{0, 2147483647, 0}>
    {
      halp_meta(description, "Index of the first element shown")
    } first;
    struct : halp::toggle_t<bool, "Overview", halp::toggle_setup{.init = false}>
    {
      halp_meta(
          description,
          "When more elements follow the first than Max elements, show elements spread "
          "evenly over all of them, and the extent of every one")
    } overview;
  } inputs;

  struct
  {
  } outputs;

  std::shared_ptr<BufferMonitor> monitor = std::make_shared<BufferMonitor>();

  Node();
  ~Node();

  void inputAboutToFinish(
      score::gfx::RenderList& r, const score::gfx::Port& p, QRhiResourceUpdateBatch*& res);
  void release(score::gfx::RenderList& r);
  void operator()() { }

  //! The layout the controls describe.
  Layout layout() const noexcept;

private:
  struct Readback;
  void decodeReadback();
  void publishEmpty(int64_t buffer_bytes, int64_t available, int64_t first, const Layout& l);

  std::shared_ptr<Readback> m_readback;
};

struct Layer : public Process::EffectLayerView
{
  Layer(
      const Process::ProcessModel& process, const Process::Context& doc,
      QGraphicsItem* parent);

  void paint_impl(QPainter* p) const override;
  //! A double click fits the scales to the elements shown now
  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* ev) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* ev) override;

private:
  Mode mode() const;
  bool autoscale() const;
  int components() const noexcept;
  std::pair<float, float> rangeOf(int c) const;

  void refresh(BufferMonitor& mon);
  //! The extent of each component in the current frame
  void measure();
  void reset();

  QRectF plotRect() const;
  void paintCurves(QPainter* p) const;
  void paintTable(QPainter* p) const;
  void paintImage(QPainter* p) const;
  void paintHistogram(QPainter* p) const;
  //! What the buffer holds, under the mouse: in the top left corner of
  //! `plot`, or its bottom right
  void paintInfo(
      QPainter* p, const QRectF& plot, const QStringList& extra, bool bottom = false) const;
  //! Where the image of the elements is drawn, and its size in elements
  QRectF imageRect(QSize& size) const;
  void buildImage() const;
  QString formatValue(float v) const;
  //! Index in the buffer of the element `i` of the frame
  int64_t indexOf(int64_t i) const noexcept;

  const Process::ControlInlet* m_mode{};
  const Process::ControlInlet* m_width{};
  const Process::ControlInlet* m_autoscale{};
  Process::MonitorLink* m_link{};
  Ui::Monitor::DoubleClick m_doubleClick;

  Frame m_frame;
  bool m_has{};
  //! The scales: what the elements spanned since the last fit
  std::array<float, 4> m_lo{}, m_hi{};
  //! What they span in the current frame
  std::array<float, 4> m_frameLo{}, m_frameHi{};
  int64_t m_nonFinite{};
  std::optional<QPointF> m_hover;

  //! Reused by the painting, which would otherwise allocate on every paint
  mutable QPolygonF m_polyline;
  mutable QImage m_image;
  mutable bool m_imageDirty{true};
  mutable std::array<std::array<int, 64>, 4> m_bins{};
};
}
