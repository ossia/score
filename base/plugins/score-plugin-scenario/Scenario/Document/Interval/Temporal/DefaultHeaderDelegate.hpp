#pragma once
#include <Process/LayerPresenter.hpp>
#include <ossia/detail/small_vector.hpp>
#include <QGlyphRun>
namespace Dataflow { class PortItem; }
namespace Scenario
{
class DefaultHeaderDelegate final
    : public QObject
    , public Process::GraphicsShapeItem
{
  public:
    DefaultHeaderDelegate(Process::LayerPresenter& p);
    ~DefaultHeaderDelegate() override;

    void updateName();
    void updateBench(double d);
    void setSize(QSizeF sz) override;

    double minPortWidth() const;
    Process::LayerPresenter& presenter;
  private:
    void updatePorts();
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QGlyphRun m_line, m_bench;
    ossia::small_vector<Dataflow::PortItem*, 3> m_inPorts, m_outPorts;
    bool m_sel{};
};
}
