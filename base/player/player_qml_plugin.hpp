#pragma once
#include <score_player_export.h>
#include "player_impl.hpp"
#include <QQmlExtensionPlugin>
namespace score
{

class QMLPlayer : public QObject
{
  Q_OBJECT
  Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
public:
  QMLPlayer();
  ~QMLPlayer();

  int port() const;

public Q_SLOTS:
  void load(QString);
  void play();
  void stop();
  void registerDevice(ossia::qt::qml_device*);
  void setPort(int);

Q_SIGNALS:
  void portChanged(int port);

private:
  PlayerImpl m_player;
  int m_port{};
};


class SCORE_PLAYER_EXPORT PlayerPlugin : public QQmlExtensionPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
  PlayerPlugin();
  void registerTypes(const char* uri) override;
  virtual ~PlayerPlugin();
};
}
