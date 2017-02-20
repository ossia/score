#pragma once
#include <Process/LayerView.hpp>
#include <QGraphicsSimpleTextItem>
#include <QString>
#include <iscore_lib_process_export.h>

class QQuickPaintedItem;
class QPainter;
class QQuickWidget;
class QGraphicsProxyWidget;
class QQuickItem;

namespace WidgetLayer
{
class WidgetTextItem;
class ISCORE_LIB_PROCESS_EXPORT View final : public Process::LayerView
{
  Q_OBJECT
public:
  explicit View(QQuickPaintedItem* parent);

  void setWidget(QWidget*);

signals:
  void pressed();

private:
  void updateText();
  void paint_impl(QPainter*) const override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;

  QGraphicsProxyWidget* m_widg{};
};
}
