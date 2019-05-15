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
class SCORE_LIB_PROCESS_EXPORT HeaderDelegate
    : public QObject,
      public Process::GraphicsShapeItem
{
public:
  HeaderDelegate(const Process::LayerPresenter& p)
      : presenter{const_cast<Process::LayerPresenter*>(&p)}
  {
  }

  ~HeaderDelegate() override;

  virtual void updateText() = 0;

  QPointer<Process::LayerPresenter> presenter;
};

class SCORE_LIB_PROCESS_EXPORT DefaultHeaderDelegate
    : public Process::HeaderDelegate
{
public:
  DefaultHeaderDelegate(const Process::LayerPresenter& p);
  ~DefaultHeaderDelegate() override;

  void updateText() override;
  const QPen& textPen(Process::Style&, const Process::ProcessModel& model) const noexcept;

  void updateBench(double d);
  void setSize(QSizeF sz) final override;
  void on_zoomRatioChanged(ZoomRatio) final override { updateText(); }

protected:
  void updatePorts();
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  QPixmap m_line, m_bench, m_fromGlyph, m_toGlyph;
  ossia::small_vector<Dataflow::PortItem*, 3> m_inPorts, m_outPorts;
  bool m_sel{};
};

SCORE_LIB_PROCESS_EXPORT
QPixmap makeGlyphs(const QString& glyph, const QPen& pen);
}
