#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <State/Domain.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QObject>
#include <QVariant>
#include <QVector>

#include <rtmidi17/message.hpp>
#include <verdigris>
namespace JS
{
class Inlet : public QObject
{
  W_OBJECT(Inlet)

public:
  using QObject::QObject;
  virtual ~Inlet() override;
  virtual Process::Inlet* make(Id<Process::Port>&& id, QObject*) = 0;
  virtual bool is_control() const { return false; }

  W_INLINE_PROPERTY_CREF(QString, address, {}, address, setAddress, addressChanged)
};

class Outlet : public QObject
{
  W_OBJECT(Outlet)

public:
  using QObject::QObject;
  virtual ~Outlet() override;
  virtual Process::Outlet* make(Id<Process::Port>&& id, QObject*) = 0;

  W_INLINE_PROPERTY_CREF(QString, address, {}, address, setAddress, addressChanged)
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
  QVariantList values() const { return m_values; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::Inlet(id, parent);
    p->type = Process::PortType::Message;
    return p;
  }

  void clear() { m_values.clear(); }
  void setValue(QVariant value);
  void addValue(QVariant&& val) { m_values.append(std::move(val)); }

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

  void clear() { m_value = QVariant{}; }
  void setValue(QVariant value);

  W_PROPERTY(QVariant, value READ value)
};

class FloatSlider : public ValueInlet
{
  W_OBJECT(FloatSlider)

public:
  using ValueInlet::ValueInlet;
  virtual ~FloatSlider() override;
  bool is_control() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::FloatSlider{
        (float)m_min, (float)m_max, (float)m_init, objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(qreal, init, {0.5}, init, setInit, initChanged)
  W_INLINE_PROPERTY_VALUE(qreal, min,  {0.}, getMin, setMin, minChanged)
  W_INLINE_PROPERTY_VALUE(qreal, max,  {1.}, getMax, setMax, maxChanged)
};

class IntSlider : public ValueInlet
{
  W_OBJECT(IntSlider)

public:
  using ValueInlet::ValueInlet;
  virtual ~IntSlider() override;
  bool is_control() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::IntSlider{
        m_min, m_max, m_init, objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(int, init, {0}, init, setInit, initChanged)
  W_INLINE_PROPERTY_VALUE(int, min,  {0}, getMin, setMin, minChanged)
  W_INLINE_PROPERTY_VALUE(int, max,  {127}, getMax, setMax, maxChanged)
};

class Enum : public ValueInlet
{
  W_OBJECT(Enum)

public:
  using ValueInlet::ValueInlet;
  virtual ~Enum() override;
  bool is_control() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::Enum{m_choices, {}, current(), objectName(), id, parent};
  }

  auto getValues() const { return choices(); }

  std::string current() const
  {
    if (!m_choices.isEmpty() && m_index >= 0 && m_index < m_choices.size())
    {
      return m_choices[m_index].toStdString();
    }
    return {};
  }

  W_INLINE_PROPERTY_VALUE(int, index, {}, index, setIndex, indexChanged)
  W_INLINE_PROPERTY_CREF(QStringList, choices, {}, choices, setChoices,choicesChanged)
};

class Toggle : public ValueInlet
{
  W_OBJECT(Toggle)

public:
  using ValueInlet::ValueInlet;
  virtual ~Toggle() override;
  bool is_control() const override { return true; }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::Toggle{m_checked, objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(bool, checked, {}, checked, setChecked, checkedChanged)
};

class LineEdit : public ValueInlet
{
  W_OBJECT(LineEdit)

public:
  using ValueInlet::ValueInlet;
  virtual ~LineEdit() override;
  bool is_control() const override { return true; }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::LineEdit{m_text, objectName(), id, parent};
  }

  W_INLINE_PROPERTY_CREF(QString, text, {}, text, setText, textChanged)
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
    auto p = new Process::AudioInlet(id, parent);
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
    auto p = new Process::AudioOutlet(id, parent);
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

  QVariantList messages() const { return m_midi; }
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

  void add(QVector<int> m) { m_midi.push_back(std::move(m)); }
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
