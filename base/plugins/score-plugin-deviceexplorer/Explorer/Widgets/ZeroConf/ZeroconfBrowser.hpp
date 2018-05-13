#pragma once
#include <QByteArray>
#include <wobjectdefs.h>
#include <QMap>
#include <QObject>
#include <QString>
#include <memory>
#include <score_plugin_deviceexplorer_export.h>

class QAction;
class QDialog;
class QListView;
class QLineEdit;
class QSpinBox;
class QWidget;
namespace servus
{
class Servus;
namespace qt
{
class ItemModel;
}
}

class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT ZeroconfBrowser : public QObject
{
  W_OBJECT(ZeroconfBrowser)
public:
  ZeroconfBrowser(const QString& service, QWidget* parent);
  ~ZeroconfBrowser();
  QAction* makeAction();

public:
  // ip, port, other data
  void sessionSelected(QString arg_1, QString arg_2, int arg_3, QMap<QString, QByteArray> arg_4) W_SIGNAL(sessionSelected, arg_1, arg_2, arg_3, arg_4);

public:
  void accept(); W_SLOT(accept);
  void reject(); W_SLOT(reject);

private:
  QDialog* m_dialog{};
  QLineEdit* m_manualIp{};
  QSpinBox* m_manualPort{};
  QListView* m_list{};
  std::unique_ptr<servus::Servus> m_serv;
  servus::qt::ItemModel* m_model{};
};
