#pragma once
#include "player_impl.hpp"

#include <QQmlExtensionPlugin>

#include <score_player_export.h>
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

public:
  void load(QString);
  void play();
  void stop();
  void registerDevice(ossia::qt::qml_device*);
  void setPort(int);

public:
  void portChanged(int port);

private:
  PlayerImpl m_player;
  int m_port{};
};

class SCORE_PLAYER_EXPORT PlayerPlugin : public QQmlExtensionPlugin
{
  Q_OBJECT

public:
  PlayerPlugin();
  void registerTypes(const char* uri) override;
  virtual ~PlayerPlugin();
};
}
