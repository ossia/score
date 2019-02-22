#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <State/Domain.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QObject>
#include <QVariant>
#include <QVector>

#include <rtmidi17/message.hpp>
#include <wobjectdefs.h>
namespace JS
{
class Inlet : public QObject
{
  W_OBJECT(Inlet)

  QString m_address;

public:
  using QObject::QObject;
  virtual ~Inlet() override;
  virtual Process::Inlet* make(Id<Process::Port>&& id, QObject*) = 0;

  QString address() const
  {
    return m_address;
  }

  virtual bool is_control() const
  {
    return false;
  }

public:
  void setAddress(QString address)
  {
    if (m_address == address)
      return;

    m_address = address;
    addressChanged(m_address);
  };
  W_SLOT(setAddress)
public:
  void addressChanged(QString address) W_SIGNAL(addressChanged, address);

  W_PROPERTY(
      QString, address READ address WRITE setAddress NOTIFY addressChanged)
};
class Outlet : public QObject
{
  W_OBJECT(Outlet)

  QString m_address;

public:
  using QObject::QObject;
  virtual ~Outlet() override;
  virtual Process::Outlet* make(Id<Process::Port>&& id, QObject*) = 0;

  QString address() const
  {
    return m_address;
  }

public:
  void setAddress(QString address)
  {
    if (m_address == address)
      return;

    m_address = address;
    addressChanged(m_address);
  };
  W_SLOT(setAddress)
public:
  void addressChanged(QString address) W_SIGNAL(addressChanged, address);

  W_PROPERTY(
      QString, address READ address WRITE setAddress NOTIFY addressChanged)
};
struct ValueMessage
{
  W_GADGET(ValueMessage)

public:
  qreal timestamp;
  QVariant value;
  W_PROPERTY(qreal, timestamp MEMBER timestamp)
  W_PROPERTY(QVariant, value MEMBER value)
};
class ValueInlet : public Inlet
{
  W_OBJECT(ValueInlet)

  QVariant m_value;
  QVariantList m_values;

public:
  ValueInlet(QObject* parent = nullptr);
  virtual ~ValueInlet() override;
  QVariant value() const;
  QVariantList values() const
  {
    return m_values;
  }

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

  W_PROPERTY(QVariantList, values READ values)
  W_PROPERTY(QVariant, value READ value)
};

class ControlInlet : public Inlet
{
  W_OBJECT(ControlInlet)

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

  W_PROPERTY(QVariant, value READ value)
};

class FloatSlider : public ValueInlet
{
  W_OBJECT(FloatSlider)

public:
  using ValueInlet::ValueInlet;
  virtual ~FloatSlider() override;
  qreal getMin() const
  {
    return m_min;
  }
  qreal getMax() const
  {
    return m_max;
  }
  qreal init() const
  {
    return m_init;
  }
  bool is_control() const override
  {
    return true;
  }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::FloatSlider{(float)m_min, (float)m_max, (float)m_init,
                                    objectName(), id,           parent};
  }

public:
  void minChanged(qreal arg_1) W_SIGNAL(minChanged, arg_1);
  void maxChanged(qreal arg_1) W_SIGNAL(maxChanged, arg_1);
  void initChanged(qreal arg_1) W_SIGNAL(initChanged, arg_1);

public:
  void setMin(qreal m)
  {
    if (m != m_min)
    {
      m_min = m;
      minChanged(m);
    }
  };
  W_SLOT(setMin)

  void setMax(qreal m)
  {
    if (m != m_max)
    {
      m_max = m;
      maxChanged(m);
    }
  };
  W_SLOT(setMax)

  void setInit(qreal m)
  {
    if (m != m_init)
    {
      m_init = m;
      initChanged(m);
    }
  };
  W_SLOT(setInit)

private:
  qreal m_min{0.};
  qreal m_max{1.};
  qreal m_init{0.5};

  W_PROPERTY(qreal, init READ init WRITE setInit NOTIFY initChanged)
  W_PROPERTY(qreal, max READ getMax WRITE setMax NOTIFY maxChanged)
  W_PROPERTY(qreal, min READ getMin WRITE setMin NOTIFY minChanged)
};

class IntSlider : public ValueInlet
{
  W_OBJECT(IntSlider)

public:
  using ValueInlet::ValueInlet;
  virtual ~IntSlider() override;
  int getMin() const
  {
    return m_min;
  }
  int getMax() const
  {
    return m_max;
  }
  int init() const
  {
    return m_init;
  }
  bool is_control() const override
  {
    return true;
  }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::IntSlider{m_min,        m_max, m_init,
                                  objectName(), id,    parent};
  }

public:
  void minChanged(int arg_1) W_SIGNAL(minChanged, arg_1);
  void maxChanged(int arg_1) W_SIGNAL(maxChanged, arg_1);
  void initChanged(int arg_1) W_SIGNAL(initChanged, arg_1);

public:
  void setMin(int m)
  {
    if (m != m_min)
    {
      m_min = m;
      minChanged(m);
    }
  };
  W_SLOT(setMin)

  void setMax(int m)
  {
    if (m != m_max)
    {
      m_max = m;
      maxChanged(m);
    }
  };
  W_SLOT(setMax)

  void setInit(int m)
  {
    if (m != m_init)
    {
      m_init = m;
      initChanged(m);
    }
  };
  W_SLOT(setInit)

private:
  int m_min{0};
  int m_max{127};
  int m_init{0};

  W_PROPERTY(int, init READ init WRITE setInit NOTIFY initChanged)
  W_PROPERTY(int, max READ getMax WRITE setMax NOTIFY maxChanged)
  W_PROPERTY(int, min READ getMin WRITE setMin NOTIFY minChanged)
};

class Enum : public ValueInlet
{
  W_OBJECT(Enum)

public:
  using ValueInlet::ValueInlet;
  virtual ~Enum() override;
  int index() const
  {
    return m_index;
  }
  QStringList choices() const
  {
    return m_choices;
  }
  bool is_control() const override
  {
    return true;
  }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::Enum{m_choices, current(), objectName(), id, parent};
  }

  auto getValues() const
  {
    return choices();
  }

  std::string current() const
  {
    if (!m_choices.isEmpty() && m_index >= 0 && m_index < m_choices.size())
    {
      return m_choices[m_index].toStdString();
    }
    return {};
  }

public:
  void choicesChanged(QStringList arg_1) W_SIGNAL(choicesChanged, arg_1);
  void indexChanged(int arg_1) W_SIGNAL(indexChanged, arg_1);

public:
  void setChoices(const QStringList& m)
  {
    if (m != m_choices)
    {
      m_choices = m;
      choicesChanged(m);
    }
  };
  W_SLOT(setChoices)

  void setIndex(int m)
  {
    if (m != m_index)
    {
      m_index = m;
      indexChanged(m);
    }
  };
  W_SLOT(setIndex)

private:
  QStringList m_choices{};
  int m_index{};

  W_PROPERTY(int, index READ index WRITE setIndex NOTIFY indexChanged)
  W_PROPERTY(
      QStringList, choices READ choices WRITE setChoices NOTIFY choicesChanged)
};

class Toggle : public ValueInlet
{
  W_OBJECT(Toggle)

public:
  using ValueInlet::ValueInlet;
  virtual ~Toggle() override;
  bool checked() const
  {
    return m_checked;
  }
  bool is_control() const override
  {
    return true;
  }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::Toggle{m_checked, objectName(), id, parent};
  }

public:
  void checkedChanged(bool arg_1) W_SIGNAL(checkedChanged, arg_1);

public:
  void setChecked(bool m)
  {
    if (m != m_checked)
    {
      m_checked = m;
      checkedChanged(m);
    }
  };
  W_SLOT(setChecked)

private:
  bool m_checked{};

  W_PROPERTY(bool, checked READ checked WRITE setChecked NOTIFY checkedChanged)
};

class LineEdit : public ValueInlet
{
  W_OBJECT(LineEdit)

public:
  using ValueInlet::ValueInlet;
  virtual ~LineEdit() override;
  QString text() const
  {
    return m_text;
  }
  bool is_control() const override
  {
    return true;
  }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::LineEdit{m_text, objectName(), id, parent};
  }

public:
  void textChanged(QString arg_1) W_SIGNAL(textChanged, arg_1);

public:
  void setText(QString m)
  {
    if (m != m_text)
    {
      m_text = m;
      textChanged(m);
    }
  };
  W_SLOT(setText)

private:
  QString m_text{};

  W_PROPERTY(QString, text READ text WRITE setText NOTIFY textChanged)
};

class ValueOutlet : public Outlet
{
  W_OBJECT(ValueOutlet)

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

public:
  void setValue(QVariant value);
  W_SLOT(setValue);
  void addValue(qreal timestamp, QVariant t);
  W_SLOT(addValue);

  W_PROPERTY(QVariant, value READ value WRITE setValue)
};

class AudioInlet : public Inlet
{
  W_OBJECT(AudioInlet)

public:
  AudioInlet(QObject* parent = nullptr);
  virtual ~AudioInlet() override;
  const QVector<QVector<double>>& audio() const;
  void setAudio(const QVector<QVector<double>>& audio);

  QVector<double> channel(int i) const
  {
    if (m_audio.size() > i)
      return m_audio[i];
    return {};
  }
  W_INVOKABLE(channel);

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Inlet(id, parent);
    p->type = Process::PortType::Audio;
    return p;
  }

private:
  QVector<QVector<double>> m_audio;
};

class AudioOutlet : public Outlet
{
  W_OBJECT(AudioOutlet)

public:
  AudioOutlet(QObject* parent = nullptr);
  virtual ~AudioOutlet() override;
  Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Outlet(id, parent);
    p->type = Process::PortType::Audio;
    if (id.val() == 0)
      p->setPropagate(true);
    return p;
  }

  const QVector<QVector<double>>& audio() const;

  void setChannel(int i, QVector<double> v)
  {
    i = std::abs(i);
    m_audio.resize(std::max(i + 1, (int)m_audio.size()));
    m_audio[i] = v;
  }
  W_INVOKABLE(setChannel)
private:
  QVector<QVector<double>> m_audio;
};

class MidiMessage
{
  W_GADGET(MidiMessage)

public:
  QByteArray bytes;

  W_PROPERTY(QByteArray, bytes MEMBER bytes)
};

class MidiInlet : public Inlet
{
  W_OBJECT(MidiInlet)

public:
  MidiInlet(QObject* parent = nullptr);
  virtual ~MidiInlet() override;
  template <typename T>
  void setMidi(const T& arr)
  {
    m_midi.clear();
    for (const rtmidi::message& mess : arr)
    {
      const auto N = mess.size();
      QVector<int> m;
      m.resize(N);

      for (std::size_t i = 0; i < N; i++)
        m[i] = mess.bytes[i];

      m_midi.push_back(QVariant::fromValue(m));
    }
  }

  QVariantList messages() const
  {
    return m_midi;
  }
  W_INVOKABLE(messages);

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Inlet(id, parent);
    p->type = Process::PortType::Midi;
    return p;
  }

private:
  QVariantList m_midi;
};

class MidiOutlet : public Outlet
{
  W_OBJECT(MidiOutlet)

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

  void setMessages(const QVariantList m)
  {
    m_midi.clear();
    for (auto& v : m)
    {
      if (v.canConvert<QVector<int>>())
        m_midi.push_back(v.value<QVector<int>>());
    }
  }
  W_INVOKABLE(setMessages);

  void add(QVector<int> m)
  {
    m_midi.push_back(std::move(m));
  }
  W_INVOKABLE(add);

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

W_REGISTER_ARGTYPE(JS::ValueInlet*)
W_REGISTER_ARGTYPE(JS::ValueOutlet*)
W_REGISTER_ARGTYPE(JS::AudioInlet*)
W_REGISTER_ARGTYPE(JS::AudioOutlet*)
W_REGISTER_ARGTYPE(JS::MidiMessage)
W_REGISTER_ARGTYPE(JS::MidiInlet*)
W_REGISTER_ARGTYPE(JS::MidiOutlet*)
