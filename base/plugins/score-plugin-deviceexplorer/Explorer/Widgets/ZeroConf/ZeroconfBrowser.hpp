#pragma once
#include <QObject>
#include <QString>
#include <QMap>
#include <QByteArray>
#include <memory>

#include <score_plugin_deviceexplorer_export.h>

class QAction;
class QDialog;
class QListView;
class QLineEdit;
class QSpinBox;
class QWidget;
namespace servus {
class Servus;
namespace qt { class ItemModel; }
}

class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT ZeroconfBrowser : public QObject
{
  Q_OBJECT
public:
  ZeroconfBrowser(const QString& service, QWidget* parent);
  ~ZeroconfBrowser();
  QAction* makeAction();

Q_SIGNALS:
  // ip, port, other data
  void sessionSelected(QString, QString, int, QMap<QString, QByteArray>);

public Q_SLOTS:
  void accept();
  void reject();

private:
  QDialog* m_dialog{};
  QLineEdit* m_manualIp{};
  QSpinBox* m_manualPort{};
  QListView* m_list{};
  std::unique_ptr<servus::Servus> m_serv;
  servus::qt::ItemModel* m_model{};
};
