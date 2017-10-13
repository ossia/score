#pragma once
#include <QObject>
#include <QVariant>
namespace JS
{
class ValueInlet: public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
  QVariant m_value;

public:
  ValueInlet(QObject* parent = nullptr);
  virtual ~ValueInlet();
  QVariant value() const;
public slots:
  void setValue(QVariant value);
signals:
  void valueChanged(QVariant value);
};

class ValueOutlet: public QObject
{
  Q_OBJECT
  Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
  QVariant m_value;

public:
  ValueOutlet(QObject* parent = nullptr);
  virtual ~ValueOutlet();
  QVariant value() const;
public slots:
  void setValue(QVariant value);
signals:
  void valueChanged(QVariant value);
};
}
Q_DECLARE_METATYPE(JS::ValueInlet*)
Q_DECLARE_METATYPE(JS::ValueOutlet*)
