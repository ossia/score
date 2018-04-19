#pragma once
#include <ossia/detail/small_vector.hpp>

#include <Process/LayerPresenter.hpp>
namespace Dataflow
{
class PortItem;
}
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT HeaderDelegate
    : public QObject
    , public Process::GraphicsShapeItem
{
public:
  HeaderDelegate(Process::LayerPresenter& p) : presenter{&p}
  {
  }

  ~HeaderDelegate() override;

  QPointer<Process::LayerPresenter> presenter;
};

class SCORE_LIB_PROCESS_EXPORT DefaultHeaderDelegate final
    : public Process::HeaderDelegate
{
public:
  DefaultHeaderDelegate(Process::LayerPresenter& p);
  ~DefaultHeaderDelegate() override;

  virtual void updateName();
  void updateBench(double d);
  void setSize(QSizeF sz) override;
  void on_zoomRatioChanged(ZoomRatio) override
  {
    updateName();
  }

private:
  void updatePorts();
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  QImage m_line, m_bench;
  ossia::small_vector<Dataflow::PortItem*, 3> m_inPorts, m_outPorts;
  bool m_sel{};
};
}
