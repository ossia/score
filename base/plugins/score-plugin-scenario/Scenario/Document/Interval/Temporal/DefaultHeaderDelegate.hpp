#pragma once
#include <Process/LayerPresenter.hpp>
#include <chobo/small_vector.hpp>
#include <QTextLayout>
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
    QTextLayout m_textcache;
    chobo::small_vector<Dataflow::PortItem*, 3> m_inPorts, m_outPorts;
};
}
