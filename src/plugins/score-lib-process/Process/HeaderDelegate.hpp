#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <ossia/detail/small_vector.hpp>

namespace Dataflow
{
class PortItem;
}
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT HeaderDelegate : public QObject, public Process::GraphicsShapeItem
{
public:
  HeaderDelegate(
      const Process::ProcessModel& m,
      const Process::Context& doc)
      : m_model{m}, m_context{doc}
  {
  }

  ~HeaderDelegate() override;

  virtual void updateText() = 0;

  const Process::ProcessModel& m_model;
  const Process::Context& m_context;
};

class SCORE_LIB_PROCESS_EXPORT DefaultHeaderDelegate : public Process::HeaderDelegate
{
public:
  DefaultHeaderDelegate(
      const Process::ProcessModel& m,
      const Process::Context& doc);
  ~DefaultHeaderDelegate() override;

  void updateText() override;
  const QPen& textPen(Process::Style&, const Process::ProcessModel& model) const noexcept;

  void updateBench(double d);
  void setSize(QSizeF sz) final override;
  void on_zoomRatioChanged(ZoomRatio) final override { updateText(); }

protected:
  void updatePorts();
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  QPixmap m_line, m_bench;
  QGraphicsItem* m_ui{};
  ossia::small_vector<Dataflow::PortItem*, 3> m_inPorts;
  double m_portEndX{};
  bool m_sel{};

  QString m_lastText;
  const QPen* m_lastPen{};
};

class SCORE_LIB_PROCESS_EXPORT FooterDelegate : public QObject, public QGraphicsItem
{
public:
  FooterDelegate(const Process::ProcessModel& m, const Process::Context& doc);

  ~FooterDelegate() override;

  virtual void setSize(QSizeF sz) = 0;
  QRectF boundingRect() const final override;

protected:
  const Process::ProcessModel& m_model;
  const Process::Context& m_context;
  QSizeF m_size{};

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  int type() const override;
};

class SCORE_LIB_PROCESS_EXPORT DefaultFooterDelegate : public Process::FooterDelegate
{
public:
  DefaultFooterDelegate(const Process::ProcessModel& m, const Process::Context& doc);
  ~DefaultFooterDelegate() override;

  void setSize(QSizeF sz) final override;

protected:
  void updatePorts();
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  ossia::small_vector<Dataflow::PortItem*, 3> m_outPorts;
  double m_portEndX{};
};

SCORE_LIB_PROCESS_EXPORT
QPixmap makeGlyphs(const QString& glyph, const QPen& pen);
}
