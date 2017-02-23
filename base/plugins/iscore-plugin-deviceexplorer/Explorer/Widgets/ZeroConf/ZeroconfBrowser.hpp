#pragma once
#include <QObject>
#include <QString>
#include <iscore_plugin_deviceexplorer_export.h>

class QAction;
class QDialog;
class QListView;
class QWidget;
namespace servus {
class Servus;
namespace qt { class ItemModel; }
}

class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT ZeroconfBrowser : public QObject
{
  Q_OBJECT
public:
  ZeroconfBrowser(const QString& service, QWidget* parent);
  ~ZeroconfBrowser();
  QAction* makeAction();

signals:
  // ip, port, other data
  void sessionSelected(QString, QString, int, QMap<QString, QByteArray>);

public slots:
  void accept();
  void reject();

private:
  QDialog* m_dialog{};
  QListView* m_list{};
  std::unique_ptr<servus::Servus> m_serv;
  servus::qt::ItemModel* m_model{};
};
