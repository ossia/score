#pragma once
#include <Process/LayerPresenter.hpp>
#include <ossia/detail/small_vector.hpp>
#include <QGlyphRun>
namespace Dataflow { class PortItem; }
namespace Process
{
class HeaderDelegate
    : public QObject
    , public Process::GraphicsShapeItem
{
  public:
    HeaderDelegate(Process::LayerPresenter& p)
      : presenter{&p}
    {

    }
    ~HeaderDelegate() override
    {

    }

    enum Shape { MiniShape, MaxiShape };
    virtual Shape headerShape(double w) const = 0;

    QPointer<Process::LayerPresenter> presenter;
};
}
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
