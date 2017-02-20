#pragma once

#include <Process/LayerView.hpp>
#include <QString>
#include <QTextLayout>

class QQuickPaintedItem;
class QPainter;
class QMimeData;

namespace Automation
{
class LayerView final : public Process::LayerView
{
  Q_OBJECT
public:
  explicit LayerView(QQuickPaintedItem* parent);
  virtual ~LayerView();

  void setDisplayedName(const QString& s);
  void showName(bool b)
  {
    m_showName = b;
    update();
  }

signals:
  void dropReceived(const QMimeData& mime);

protected:
  void paint_impl(QPainter* painter) const override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

private:
  bool m_showName{true};

  QTextLayout m_textcache;
};
}
