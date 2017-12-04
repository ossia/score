#pragma once

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
  void updateMin(double f = 0.);
  void updateMax(double f = 0.);
  void updateUnit();

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
  void on_zoomRatioChanged(ZoomRatio) override;
  int size();

private:
  void updateCache(QTextLayout& cache, const QString& txt);

  Process::LayerPresenter& presenter;
  QTextLayout m_textcache;
  QTextLayout m_mincache;
  QTextLayout m_maxcache;
  QTextLayout m_unitcache;

  int m_gap{10};
};
}
