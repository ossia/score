#pragma once
#include <Process/Dataflow/Port.hpp>
#include <ModernMIDI/midi_message.h>
#include <ossia/network/domain/domain.hpp>
#include <State/Domain.hpp>
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
    virtual ~Inlet() override;
    virtual Process::Inlet* make(Id<Process::Port>&& id, QObject*) = 0;

};
class Outlet: public QObject
{
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual ~Outlet() override;
    virtual Process::Outlet* make(Id<Process::Port>&& id, QObject*) = 0;

};
class ValueInlet: public Inlet
{
  Q_OBJECT
  Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
  QVariant m_value;

public:
  ValueInlet(QObject* parent = nullptr);
  virtual ~ValueInlet() override;
  QVariant value() const;

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Inlet(id, parent);
    p->type = Process::PortType::Message;
    return p;
  }
public slots:
  void setValue(QVariant value);
signals:
  void valueChanged(QVariant value);
};


class FloatSlider: public ValueInlet
{
  Q_OBJECT
    Q_PROPERTY(qreal min READ getMin WRITE setMin NOTIFY minChanged)
    Q_PROPERTY(qreal max READ getMax WRITE setMax NOTIFY maxChanged)
    Q_PROPERTY(qreal init READ init WRITE setInit NOTIFY initChanged)

  public:
    using ValueInlet::ValueInlet;
    virtual ~FloatSlider() override;
    qreal getMin() const { return m_min; }
    qreal getMax() const { return m_max; }
    qreal init() const { return m_init; }

    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(m_init);
      p->setDomain(ossia::make_domain(m_min, m_max));
      return p;
    }

  signals:
    void minChanged(qreal);
    void maxChanged(qreal);
    void initChanged(qreal);

  public slots:
    void setMin(qreal m)
    {
      if(m != m_min)
      {
        m_min = m;
        emit minChanged(m);
      }
    }

    void setMax(qreal m)
    {
      if(m != m_max)
      {
        m_max = m;
        emit maxChanged(m);
      }
    }

    void setInit(qreal m)
    {
      if(m != m_init)
      {
        m_init = m;
        emit initChanged(m);
      }
    }
  private:
    qreal m_min{};
    qreal m_max{};
    qreal m_init{};
};

class IntSlider: public ValueInlet
{
  Q_OBJECT
    Q_PROPERTY(int min READ getMin WRITE setMin NOTIFY minChanged)
    Q_PROPERTY(int max READ getMax WRITE setMax NOTIFY maxChanged)
    Q_PROPERTY(int init READ init WRITE setInit NOTIFY initChanged)

  public:
    using ValueInlet::ValueInlet;
    virtual ~IntSlider() override;
    int getMin() const { return m_min; }
    int getMax() const { return m_max; }
    int init() const { return m_init; }

    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(m_init);
      p->setDomain(ossia::make_domain(m_min, m_max));
      return p;
    }

  signals:
    void minChanged(int);
    void maxChanged(int);
    void initChanged(int);

  public slots:
    void setMin(int m)
    {
      if(m != m_min)
      {
        m_min = m;
        emit minChanged(m);
      }
    }

    void setMax(int m)
    {
      if(m != m_max)
      {
        m_max = m;
        emit maxChanged(m);
      }
    }

    void setInit(int m)
    {
      if(m != m_init)
      {
        m_init = m;
        emit initChanged(m);
      }
    }
  private:
    int m_min{};
    int m_max{};
    int m_init{};
};

class Enum: public ValueInlet
{
  Q_OBJECT
    Q_PROPERTY(QStringList choices READ choices WRITE setChoices NOTIFY choicesChanged)

    Q_PROPERTY(int index READ index WRITE setIndex NOTIFY indexChanged)

  public:
    using ValueInlet::ValueInlet;
    virtual ~Enum() override;
    int index() const { return m_index; }
    QStringList choices() const { return m_choices; }

    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(current());
      return p;
    }

    auto getValues() const { return choices(); }

    std::string current() const
    {
      if(!m_choices.isEmpty() && m_index >= 0 && m_index < m_choices.size())
      {
        return m_choices[m_index].toStdString();
      }
      return {};
    }

  signals:
    void choicesChanged(QStringList);
    void indexChanged(int);

  public slots:
    void setChoices(const QStringList& m)
    {
      if(m != m_choices)
      {
        m_choices = m;
        emit choicesChanged(m);
      }
    }

    void setIndex(int m)
    {
      if(m != m_index)
      {
        m_index = m;
        emit indexChanged(m);
      }
    }

  private:
    QStringList m_choices{};
    int m_index{};
};
class Toggle: public ValueInlet
{
  Q_OBJECT
    Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY checkedChanged)

  public:
    using ValueInlet::ValueInlet;
    virtual ~Toggle() override;
    bool checked() const { return m_checked; }
    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(m_checked);
      return p;
    }

  signals:
    void checkedChanged(bool);

  public slots:
    void setChecked(bool m)
    {
      if(m != m_checked)
      {
        m_checked = m;
        emit checkedChanged(m);
      }
    }

  private:
    bool m_checked{};
};

class LineEdit: public ValueInlet
{
  Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)

  public:
    using ValueInlet::ValueInlet;
    virtual ~LineEdit() override;
    QString text() const { return m_text; }
    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(m_text.toStdString());
      return p;
    }

  signals:
    void textChanged(QString);

  public slots:
    void setText(QString m)
    {
      if(m != m_text)
      {
        m_text = m;
        emit textChanged(m);
      }
    }

  private:
    QString m_text{};
};

class ValueOutlet: public Outlet
{
  Q_OBJECT
  Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
  QVariant m_value;

public:
  ValueOutlet(QObject* parent = nullptr);
  virtual ~ValueOutlet() override;
  QVariant value() const;
  Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Outlet(id, parent);
    p->type = Process::PortType::Message;
    return p;
  }
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
    virtual ~AudioInlet() override;
    const QVector<QVector<double>>& audio() const;
    void setAudio(const QVector<QVector<double>>& audio);

    Q_INVOKABLE QVector<double> channel(int i) const {
      if(m_audio.size() > i)
        return m_audio[i];
      return {};
    }

    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::Inlet(id, parent);
      p->type = Process::PortType::Audio;
      return p;
    }
  private:
    QVector<QVector<double>> m_audio;
};

class AudioOutlet: public Outlet
{
  Q_OBJECT

  public:
    AudioOutlet(QObject* parent = nullptr);
    virtual ~AudioOutlet() override;
    Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::Outlet(id, parent);
      p->type = Process::PortType::Audio;
      if(id.val() == 0)
        p->setPropagate(true);
      return p;
    }

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

class MidiMessage
{
  Q_GADGET
  Q_PROPERTY(QByteArray bytes MEMBER bytes)
public:
  QByteArray bytes;
};

class MidiInlet: public Inlet
{
  Q_OBJECT

  public:
    MidiInlet(QObject* parent = nullptr);
    virtual ~MidiInlet() override;
    template<typename T>
    void setMidi(const T& arr)
    {
      m_midi.clear();
      for(const mm::MidiMessage& mess : arr)
      {
        QVector<int> m;
        for(auto byte : mess.data)
          m.push_back(byte);
        m_midi.push_back(QVariant::fromValue(m));
      }
    }

    Q_INVOKABLE QVariantList messages() const {
      return m_midi;
    }

    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::Inlet(id, parent);
      p->type = Process::PortType::Midi;
      return p;
    }
  private:
    QVariantList m_midi;
};

class MidiOutlet: public Outlet
{
  Q_OBJECT

  public:
    MidiOutlet(QObject* parent = nullptr);
    virtual ~MidiOutlet() override;
    Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::Outlet(id, parent);
      p->type = Process::PortType::Midi;
      return p;
    }

    void clear();
    const QVector<QVector<int>>& midi() const;

    Q_INVOKABLE void setMessages(const QVariantList m)
    {
      m_midi.clear();
      for(auto& v : m)
      {
        if(v.canConvert<QVector<int>>())
          m_midi.push_back(v.value<QVector<int>>());
      }
    }

    Q_INVOKABLE void add(QVector<int> m)
    {
      m_midi.push_back(std::move(m));
    }
  private:
    QVector<QVector<int>> m_midi;
};

}
Q_DECLARE_METATYPE(JS::ValueInlet*)
Q_DECLARE_METATYPE(JS::ValueOutlet*)
Q_DECLARE_METATYPE(JS::AudioInlet*)
Q_DECLARE_METATYPE(JS::AudioOutlet*)
Q_DECLARE_METATYPE(JS::MidiMessage)
Q_DECLARE_METATYPE(JS::MidiInlet*)
Q_DECLARE_METATYPE(JS::MidiOutlet*)
