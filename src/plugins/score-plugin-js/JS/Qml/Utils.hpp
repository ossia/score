#pragma once
#include <JS/Qml/QmlObjects.hpp>

#include <QJSValue>
#include <QObject>
#include <QString>
#include <QTime>
#include <Process/TimeValue.hpp>

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

  QString uuid();
  W_SLOT(uuid)

  QTime toTime(TimeVal v);
  W_SLOT(toTime)
  double toMilliseconds(TimeVal v);
  W_SLOT(toMilliseconds)
  bool isInfinite(TimeVal v);
  W_SLOT(isInfinite)
};

class JsSystem : public QObject
{
  W_OBJECT(JsSystem)
public:
  bool isDeviceMDMEnrolled();
  W_SLOT(isDeviceMDMEnrolled)

  int availableCudaDevice();
  W_SLOT(availableCudaDevice)

  int availableCudaToolkitDylibs(int major, int minor);
  W_SLOT(availableCudaToolkitDylibs)
};
}
