#include "RemoteApplication.hpp"

#include <QApplication>

#include <wobjectimpl.h>
W_OBJECT_IMPL(WebSocketHandler)
RemoteApplication::RemoteApplication(int& argc, char** argv)
    : m_app{new QApplication{argc, argv}}
{
  engine.load(QUrl(QStringLiteral("qrc:/resources/main.qml")));

  auto obj = engine.rootObjects().first();

  connect(
      obj, SIGNAL(itemClicked(int)), &m_triggers, SLOT(on_rowPressed(int)));
  connect(obj, SIGNAL(play()), &m_triggers, SLOT(on_play()));
  connect(obj, SIGNAL(pause()), &m_triggers, SLOT(on_pause()));
  connect(obj, SIGNAL(stop()), &m_triggers, SLOT(on_stop()));
  connect(
      obj,
      SIGNAL(addressChanged(QString)),
      &m_triggers,
      SLOT(on_addressChanged(QString)));

  obj->setProperty("model", QVariant::fromValue(&m_triggers.m_activeSyncs));
}

RemoteApplication::~RemoteApplication() {}

int RemoteApplication::exec()
{
  return m_app->exec();
}
