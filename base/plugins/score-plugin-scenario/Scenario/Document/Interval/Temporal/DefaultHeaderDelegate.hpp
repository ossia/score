#pragma once
#include <Process/LayerPresenter.hpp>
#include <QTextLayout>
namespace Dataflow { class PortItem; }
namespace Scenario
{
class DefaultHeaderDelegate
    : public QObject
    , public Process::GraphicsShapeItem
{
  public:
    DefaultHeaderDelegate(Process::LayerPresenter& p);

    void onCreateCable(Dataflow::PortItem* p1, Dataflow::PortItem* p2);
    void updateName();

  private:
    void updatePorts();
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    Process::LayerPresenter& presenter;
    QTextLayout m_textcache;
    std::vector<Dataflow::PortItem*> m_items;
};
}
