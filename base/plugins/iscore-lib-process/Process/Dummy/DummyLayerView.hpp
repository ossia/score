#pragma once
#include <Process/LayerView.hpp>
#include <QGraphicsSimpleTextItem>
#include <QString>
#include <iscore_lib_process_export.h>

class QQuickPaintedItem;
class QPainter;
class QQuickWidget;
class QQuickItem;

namespace Dummy
{
class DummyTextItem;
class ISCORE_LIB_PROCESS_EXPORT DummyLayerView final
    : public Process::LayerView
{
  Q_OBJECT
public:
  explicit DummyLayerView(QQuickPaintedItem* parent);

  void setText(const QString& text);

signals:
  void pressed();

private:
  void updateText();
  void paint_impl(QPainter*) const override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  DummyTextItem* m_text{};
  /*
  QQuickWidget* m_view{};
  QQuickItem* m_item{};
  */
};
}
