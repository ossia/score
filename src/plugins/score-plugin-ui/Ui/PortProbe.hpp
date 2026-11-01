#pragma once
#include <Process/Dataflow/PortProbe.hpp>

#include <Gfx/Widgets/TexturePreview.hpp>

#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/value/value.hpp>

#include <QPointer>

#include <boost/circular_buffer.hpp>
#include <memory>
#include <vector>

namespace Execution
{
class PortTap;
}
namespace Ui
{
/**
 * What goes through a port, live: levels and a trace for audio, the latest
 * messages for MIDI, the latest values (and a trail when they are numbers)
 * for values, a thumbnail for textures. Only while execution updates are
 * enabled; it refreshes at the UI event rate.
 */
class ProbeItem final : public Process::PortProbeItem
{
public:
  ProbeItem(const Process::Port& port, const Process::Context& ctx);
  ~ProbeItem() override;

  QRectF boundingRect() const override;
  void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override;

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* ev) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* ev) override;
  void timerEvent(QTimerEvent* ev) override;

private:
  //! Taps the port when execution runs, the texture outlet for textures.
  void attach();
  //! Takes what came since the last time; false when nothing did.
  bool drain();
  //! A view for what the unit of the values means; false when there is none.
  bool paintUnit(QPainter& p, const QRectF& r) const;

  QPointer<const Process::Port> m_port;
  const Process::Context& m_context;
  QString m_title;
  std::unique_ptr<Execution::PortTap> m_tap;
  Gfx::TexturePreview m_texture;

  // Audio: per channel, and the first channels' latest samples
  std::vector<float> m_peak, m_rms;
  std::vector<std::vector<float>> m_trace;
  std::vector<float> m_buffer;
  uint64_t m_audioFirst{};
  // MIDI and values: the latest, newest last
  boost::circular_buffer<QString> m_lines;
  boost::circular_buffer<float> m_trail;
  // Values with a unit: the latest one, and where positions went
  ossia::unit_t m_unit;
  ossia::value m_last;
  boost::circular_buffer<QPointF> m_path;
  int m_sinceAttach{};
  //! The execution update setting as last seen: what the probe says when idle
  bool m_executionUpdates{true};
};

class ProbeFactory final : public Process::PortProbeFactory
{
  SCORE_CONCRETE("4a1e7f2c-9d36-4b58-a0c1-7e3f5b2d8c91")
public:
  Process::PortProbeItem*
  make(const Process::Port& port, const Process::Context& ctx) override;
};
}
