#pragma once
#include <QBrush>
#include <QFont>
#include <QObject>
#include <QPair>
#include <QVector>
#include <boost/bimap.hpp>
#include <QPen>
#include <score_lib_base_export.h>
namespace score
{
class SCORE_LIB_BASE_EXPORT Skin : public QObject
{
  Q_OBJECT
public:
  static Skin& instance();

  void load(const QJsonObject& style);

  QFont SansFont;
  QFont MonoFont;

  QBrush Dark;
  QBrush HalfDark;
  QBrush Gray;
  QBrush HalfLight;
  QBrush Light;

  QBrush Emphasis1;
  QBrush Emphasis2;
  QBrush Emphasis3;
  QBrush Emphasis4;

  QBrush Base1;
  QBrush Base2;
  QBrush Base3;
  QBrush Base4;

  QBrush Warn1;
  QBrush Warn2;
  QBrush Warn3;

  QBrush Background1;

  QBrush Transparent1;
  QBrush Transparent2;
  QBrush Transparent3;

  QBrush Smooth1;
  QBrush Smooth2;
  QBrush Smooth3;

  QBrush Tender1;
  QBrush Tender2;
  QBrush Tender3;

  QBrush Pulse1;
  QBrush Pulse2;

  const QPen TransparentPen;
  const QBrush TransparentBrush;
  const QPen NoPen;
  const QBrush NoBrush;
  QBrush TextBrush;

  const QBrush* fromString(const QString& s) const;
  QBrush* fromString(const QString& s);
  QString toString(const QBrush*) const;

  QVector<QPair<QColor, QString>> getColors() const;

Q_SIGNALS:
  void changed();

private:
  void timerEvent(QTimerEvent *event) override;
  Skin() noexcept;

  boost::bimap<QString, QBrush*> m_colorMap;

  bool m_pulseDirection{false};
};
}
