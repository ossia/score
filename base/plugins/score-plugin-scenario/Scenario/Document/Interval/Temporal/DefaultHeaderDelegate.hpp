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

    void updateName();
    void setSize(QSizeF sz) override;

    double minPortWidth() const;
  private:
    void updatePorts();
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    Process::LayerPresenter& presenter;
    QGlyphRun m_line;
    ossia::small_vector<Dataflow::PortItem*, 3> m_inPorts, m_outPorts;
};
}
