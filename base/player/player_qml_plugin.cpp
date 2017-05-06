#include "player_qml_plugin.hpp"
#include <ossia-qt/qml_plugin.hpp>
#include <QQmlEngine>

namespace iscore
{


QMLPlayer::QMLPlayer()
{
  registerDevice(&ossia::qt::qml_singleton_device::instance());
}

QMLPlayer::~QMLPlayer()
{

}

int QMLPlayer::port() const
{
  return m_port;
}

void QMLPlayer::load(QString str)
{
  m_player.sig_loadFile(str);
}

void QMLPlayer::play()
{
  m_player.sig_play();
}

void QMLPlayer::stop()
{
  m_player.sig_stop();
}

void QMLPlayer::registerDevice(ossia::qt::qml_device* dev)
{
  if(dev)
    m_player.registerDevice(&dev->device());
}

void QMLPlayer::setPort(int p)
{
  m_port = p;
  m_player.sig_setPort(p);
}


PlayerPlugin::PlayerPlugin()
{
}

void PlayerPlugin::registerTypes(const char* uri)
{
  qmlRegisterType<iscore::QMLPlayer>(uri, 1, 0, "Player");
  ossia::qt::qml_plugin::reg(uri);
}

PlayerPlugin::~PlayerPlugin()
{

}

}
