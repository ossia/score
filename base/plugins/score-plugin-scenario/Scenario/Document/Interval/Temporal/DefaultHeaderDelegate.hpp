#pragma once
#include <Process/HeaderDelegate.hpp>
#include <ossia/detail/small_vector.hpp>
namespace Dataflow { class PortItem; }
namespace Scenario
{
class DefaultHeaderDelegate final
    : public Process::HeaderDelegate
{
  public:
    DefaultHeaderDelegate(Process::LayerPresenter& p);
    ~DefaultHeaderDelegate() override;

    Shape headerShape(double w) const override;
    void updateName();
    void updateBench(double d);
    void setSize(QSizeF sz) override;
    void on_zoomRatioChanged(ZoomRatio) override
    {
      updateName();
    }

    double minPortWidth() const;
  private:
    void updatePorts();
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QImage m_line, m_bench;
    ossia::small_vector<Dataflow::PortItem*, 3> m_inPorts, m_outPorts;
    bool m_sel{};
};
}
