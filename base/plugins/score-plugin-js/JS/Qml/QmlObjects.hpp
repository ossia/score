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
  Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)
  QString m_address;

public:
    using QObject::QObject;
    virtual ~Inlet() override;
    virtual Process::Inlet* make(Id<Process::Port>&& id, QObject*) = 0;

  QString address() const
  {
    return m_address;
  }
  
  virtual bool is_control() const { return false; }
public Q_SLOTS:
  void setAddress(QString address)
  {
    if (m_address == address)
      return;

    m_address = address;
    addressChanged(m_address);
  }
Q_SIGNALS:
  void addressChanged(QString address);
};
class Outlet: public QObject
{
    Q_OBJECT
  Q_PROPERTY(QString address READ address WRITE setAddress NOTIFY addressChanged)
  QString m_address;

public:
    using QObject::QObject;
    virtual ~Outlet() override;
    virtual Process::Outlet* make(Id<Process::Port>&& id, QObject*) = 0;

  QString address() const
  {
    return m_address;
  }
public Q_SLOTS:
  void setAddress(QString address)
  {
    if (m_address == address)
      return;

    m_address = address;
    addressChanged(m_address);
  }
Q_SIGNALS:
  void addressChanged(QString address);
};
struct ValueMessage
{
    Q_GADGET
    Q_PROPERTY(qreal timestamp MEMBER timestamp)
    Q_PROPERTY(QVariant value MEMBER value)

  public:
    qreal timestamp;
    QVariant value;
};
class ValueInlet: public Inlet
{
  Q_OBJECT
  Q_PROPERTY(QVariant value READ value)
  Q_PROPERTY(QVariantList values READ values)
  QVariant m_value;
  QVariantList m_values;

public:
  ValueInlet(QObject* parent = nullptr);
  virtual ~ValueInlet() override;
  QVariant value() const;
  QVariantList values() const { return m_values; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Inlet(id, parent);
    p->type = Process::PortType::Message;
    return p;
  }

  void clear()
  {
    m_values.clear();
  }
  void setValue(QVariant value);
  void addValue(QVariant&& val)
  {
    m_values.append(std::move(val));
  }
};


class ControlInlet: public Inlet
{
  Q_OBJECT
  Q_PROPERTY(QVariant value READ value)
  QVariant m_value;

public:
  ControlInlet(QObject* parent = nullptr);
  virtual ~ControlInlet() override;
  QVariant value() const;

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Inlet(id, parent);
    p->type = Process::PortType::Message;
    return p;
  }

  void clear()
  {
    m_value = QVariant{};
  }
  void setValue(QVariant value);
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
    bool is_control() const override { return true; }

    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(m_init);
      p->setDomain(ossia::make_domain(m_min, m_max));
      return p;
    }

  Q_SIGNALS:
    void minChanged(qreal);
    void maxChanged(qreal);
    void initChanged(qreal);

  public Q_SLOTS:
    void setMin(qreal m)
    {
      if(m != m_min)
      {
        m_min = m;
        minChanged(m);
      }
    }

    void setMax(qreal m)
    {
      if(m != m_max)
      {
        m_max = m;
        maxChanged(m);
      }
    }

    void setInit(qreal m)
    {
      if(m != m_init)
      {
        m_init = m;
        initChanged(m);
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
    bool is_control() const override { return true; }

    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(m_init);
      p->setDomain(ossia::make_domain(m_min, m_max));
      return p;
    }

  Q_SIGNALS:
    void minChanged(int);
    void maxChanged(int);
    void initChanged(int);

  public Q_SLOTS:
    void setMin(int m)
    {
      if(m != m_min)
      {
        m_min = m;
        minChanged(m);
      }
    }

    void setMax(int m)
    {
      if(m != m_max)
      {
        m_max = m;
        maxChanged(m);
      }
    }

    void setInit(int m)
    {
      if(m != m_init)
      {
        m_init = m;
        initChanged(m);
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
    bool is_control() const override { return true; }

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

  Q_SIGNALS:
    void choicesChanged(QStringList);
    void indexChanged(int);

  public Q_SLOTS:
    void setChoices(const QStringList& m)
    {
      if(m != m_choices)
      {
        m_choices = m;
        choicesChanged(m);
      }
    }

    void setIndex(int m)
    {
      if(m != m_index)
      {
        m_index = m;
        indexChanged(m);
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
    bool is_control() const override { return true; }
    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(m_checked);
      return p;
    }

  Q_SIGNALS:
    void checkedChanged(bool);

  public Q_SLOTS:
    void setChecked(bool m)
    {
      if(m != m_checked)
      {
        m_checked = m;
        checkedChanged(m);
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
    bool is_control() const override { return true; }
    Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
    {
      auto p = new Process::ControlInlet(id, parent);
      p->type = Process::PortType::Message;
      p->setValue(m_text.toStdString());
      return p;
    }

  Q_SIGNALS:
    void textChanged(QString);

  public Q_SLOTS:
    void setText(QString m)
    {
      if(m != m_text)
      {
        m_text = m;
        textChanged(m);
      }
    }

  private:
    QString m_text{};
};

class ValueOutlet: public Outlet
{
  Q_OBJECT
  Q_PROPERTY(QVariant value READ value WRITE setValue)
  QVariant m_value;

public:
  std::vector<ValueMessage> values;

  ValueOutlet(QObject* parent = nullptr);
  virtual ~ValueOutlet() override;
  QVariant value() const;
  void clear()
  {
    m_value = QVariant{};
    values.clear();
  }
  Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Outlet(id, parent);
    p->type = Process::PortType::Message;
    return p;
  }
public Q_SLOTS:
  void setValue(QVariant value);
  void addValue(qreal timestamp, QVariant t);
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
