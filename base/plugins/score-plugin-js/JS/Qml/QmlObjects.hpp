#pragma once
#include <QObject>
#include <QVariant>
#include <QVector>
namespace JS
{
class Inlet: public QObject
{
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual ~Inlet();

};
class Outlet: public QObject
{
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual ~Outlet();

};
class ValueInlet: public Inlet
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

class ValueOutlet: public Outlet
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

class AudioInlet: public Inlet
{
  Q_OBJECT

  public:
    AudioInlet(QObject* parent = nullptr);
    virtual ~AudioInlet();
    const QVector<QVector<double>>& audio() const;
    void setAudio(const QVector<QVector<double>>& audio);

    Q_INVOKABLE QVector<double> channel(int i) const { return m_audio[i]; }

  private:
    QVector<QVector<double>> m_audio;
};

class AudioOutlet: public Outlet
{
  Q_OBJECT

  public:
    AudioOutlet(QObject* parent = nullptr);
    virtual ~AudioOutlet();
    const QVector<QVector<double>>& audio() const;

    Q_INVOKABLE void setChannel(int i, QVector<double> v )
    {
      i = std::abs(i);
      m_audio.resize(std::max(i+1, (int)m_audio.size()));
      m_audio[i] = v;
    }

  private:
    QVector<QVector<double>> m_audio;
};

}
Q_DECLARE_METATYPE(JS::ValueInlet*)
Q_DECLARE_METATYPE(JS::ValueOutlet*)
Q_DECLARE_METATYPE(JS::AudioInlet*)
Q_DECLARE_METATYPE(JS::AudioOutlet*)
