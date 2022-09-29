#pragma once
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

  QString layoutTextLines(QString text, QString font, int pointSize, int maxWidth);
  W_SLOT(layoutTextLines)
};
}
