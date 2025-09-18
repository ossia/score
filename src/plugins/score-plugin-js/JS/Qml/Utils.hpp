#pragma once
#include <JS/Qml/QmlObjects.hpp>

#include <QJSValue>
#include <QObject>
#include <QString>

#include <verdigris>
namespace JS
{
class JsUtils : public QObject
{
  W_OBJECT(JsUtils)
public:
  QByteArray readFile(QString path);
  W_SLOT(readFile)
  void writeFile(QString path, QByteArray content);
  W_SLOT(writeFile)

  void shell(QString cmd, QJSValue onFinish);
  W_SLOT(shell)

  QString layoutTextLines(QString text, QString font, int pointSize, int maxWidth);
  W_SLOT(layoutTextLines)
};
}
