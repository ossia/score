#pragma once
#include <JS/Qml/QmlObjects.hpp>
#include <Library/LibrarySettings.hpp>

#include <ossia/detail/logger.hpp>

#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QUrl>

namespace JS
{
inline JS::Script* createJSObject(const QString& val, QQmlEngine* m_engine)
{
  if(val.trimmed().startsWith("import"))
  {
    QQmlComponent c{m_engine};
    {
      auto& lib = score::AppContext().settings<Library::Settings::Model>();
      // FIXME QTBUG-107204
      QString path = lib.getDefaultLibraryPath() + QDir::separator() + "Scripts"
                     + QDir::separator() + "include" + QDir::separator();

      c.setData(val.toUtf8(), QUrl::fromLocalFile(path));
    }
    const auto& errs = c.errors();
    if(!errs.empty())
    {
      ossia::logger().error(
          "Uncaught exception at line {} : {}", errs[0].line(),
          errs[0].toString().toStdString());
    }
    else
    {
      auto object = c.create();
      auto obj = qobject_cast<JS::Script*>(object);
      if(obj)
        return obj;
      delete object;
      return nullptr;
    }
  }
  else
  {
    qDebug() << "URL: " << val << QUrl::fromLocalFile(val);
    QQmlComponent c{m_engine, QUrl::fromLocalFile(val)};
    const auto& errs = c.errors();
    if(!errs.empty())
    {
      ossia::logger().error(
          "Uncaught exception at line {} : {}", errs[0].line(),
          errs[0].toString().toStdString());
    }
    else
    {
      auto object = c.create();
      auto obj = qobject_cast<JS::Script*>(object);
      if(obj)
        return obj;
      delete object;
      return nullptr;
    }
  }
  return nullptr;
}
}
