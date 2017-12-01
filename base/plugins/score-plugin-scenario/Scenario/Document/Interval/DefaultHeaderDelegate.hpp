#pragma ponce

#include <QTextLayout>
#include <Process/LayerPresenter.hpp>

namespace Scenario
{
class DefaultHeaderDelegate
    : public QObject
    , public Process::GraphicsShapeItem
{
public:
  DefaultHeaderDelegate(Process::LayerPresenter& p);

  void updateName();
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
  Process::LayerPresenter& presenter;
  QTextLayout m_textcache;
};
}
