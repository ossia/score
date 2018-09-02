#ifndef CENTRALITEMMODEL_H
#define CENTRALITEMMODEL_H

#include <Models/WidgetListModel.hpp>
#include <QObject>
#include <QQuickItem>
#include <RemoteContext.hpp>

class QQmlApplicationEngine;

namespace RemoteUI
{
class GUIItem;

//! Data model for the control surface: just a list of widgets
class CentralItemModel : public QObject
{
  Q_OBJECT
public:
  explicit CentralItemModel(Context&, QObject* parent = 0);

public Q_SLOTS:
  void on_itemCreated(QString data, qreal x, qreal y);
  void on_addressCreated(QString data, qreal x, qreal y);

  void addItem(GUIItem* item);
  void removeItem(GUIItem* item);

private:
  Context& m_ctx;
  QList<GUIItem*> m_guiItems;
  QQuickItem* create(WidgetKind c);
};
}
#endif // CENTRALITEMMODEL_H
