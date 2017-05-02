#pragma once
#include <iscore_player_export.h>
#include "player_impl.hpp"
#include <QQmlExtensionPlugin>
namespace iscore
{

class QMLPlayer : public QObject
{
  Q_OBJECT
public:
  QMLPlayer();
  ~QMLPlayer();

public slots:
  void load(QString);
  void play();
  void stop();
  void registerDevice(ossia::qt::qml_device*);

private:
  PlayerImpl m_player;
};


class ISCORE_PLAYER_EXPORT PlayerPlugin : public QQmlExtensionPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
  PlayerPlugin();
  void registerTypes(const char* uri) override;
  virtual ~PlayerPlugin();
};
}
