#pragma once
#include <State/Domain.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <JS/Qml/QtMetatypes.hpp>

#include <libremidi/detail/conversion.hpp>

#if defined(SCORE_HAS_GPU_JS)
#include <Gfx/TexturePort.hpp>

#include <QQuickItem>
#endif

#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/network/domain/domain.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QJSValue>
#include <QObject>
#include <QQmlListProperty>
#include <QVariant>
#include <QVector>

#include <libremidi/message.hpp>
#include <libremidi/ump.hpp>

#include <score_plugin_js_export.h>
#include <wobjectimpl.h>

#include <verdigris>

class QQuickItem;
namespace JS
{
class SCORE_PLUGIN_JS_EXPORT Inlet : public QObject
{
  W_OBJECT(Inlet)

public:
  using QObject::QObject;
  virtual ~Inlet() override;
  virtual Process::Inlet* make(Id<Process::Port>&& id, QObject*) = 0;
  virtual bool isEvent() const { return false; }

  W_INLINE_PROPERTY_CREF(QString, address, {}, address, setAddress, addressChanged)
};

class SCORE_PLUGIN_JS_EXPORT Outlet : public QObject
{
  W_OBJECT(Outlet)

public:
  using QObject::QObject;
  virtual ~Outlet() override;
  virtual Process::Outlet* make(Id<Process::Port>&& id, QObject*) = 0;

  W_INLINE_PROPERTY_CREF(QString, address, {}, address, setAddress, addressChanged)
};

struct SCORE_PLUGIN_JS_EXPORT InValueMessage
{
  W_GADGET(InValueMessage)

public:
  qreal timestamp;
  QVariant value;
  W_PROPERTY(qreal, timestamp MEMBER timestamp)
  W_PROPERTY(QVariant, value MEMBER value)
};

struct SCORE_PLUGIN_JS_EXPORT OutValueMessage
{
  W_GADGET(OutValueMessage)

public:
  qreal timestamp;
  QJSValue value;
  W_PROPERTY(qreal, timestamp MEMBER timestamp)
  W_PROPERTY(QJSValue, value MEMBER value)
};

class SCORE_PLUGIN_JS_EXPORT ValueInlet : public Inlet
{
  W_OBJECT(ValueInlet)

  QVariant m_value;
  QVariantList m_values;

public:
  explicit ValueInlet(QObject* parent = nullptr);
  virtual ~ValueInlet() override;
  QVariant value() const;
  QVariantList values() const { return m_values; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::ValueInlet(objectName(), id, parent);
  }

  int length() const noexcept;
  W_SLOT(length)

  QVariant at(int index) const noexcept;
  W_SLOT(at)

  void clear() { m_values.clear(); }
  void setValue(QVariant value);
  void addValue(QVariant&& val) { m_values.append(std::move(val)); }
  void valueChanged(QVariant value) W_SIGNAL(valueChanged, value);

  W_PROPERTY(QVariantList, values READ values)
  W_PROPERTY(QVariant, value READ value NOTIFY valueChanged)
};

class SCORE_PLUGIN_JS_EXPORT ControlInlet : public Inlet
{
  W_OBJECT(ControlInlet)

  QVariant m_value;

public:
  explicit ControlInlet(QObject* parent = nullptr);
  virtual ~ControlInlet() override;
  QVariant value() const noexcept;

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::ControlInlet(objectName(), id, parent);
  }

  void clear() { m_value = QVariant{}; }
  virtual void setValue(QVariant value);
  void valueChanged(QVariant value) W_SIGNAL(valueChanged, value);

  W_PROPERTY(QVariant, value READ value NOTIFY valueChanged)
};

template <typename Impl, typename ValueType, typename OssiaType>
class SCORE_PLUGIN_JS_EXPORT GenericControlInlet : public ControlInlet
{
  W_OBJECT(GenericControlInlet)

public:
  using ControlInlet::ControlInlet;
  virtual ~GenericControlInlet() override = default;
  bool isEvent() const override { return true; }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Impl(objectName(), id, parent);
  }

  void clear() { m_value = {}; }
  ValueType value() const noexcept { return m_value; }
  void setValue(QVariant value) override
  {
    auto conv = value.value<ValueType>();
    if(m_value == conv)
      return;

    m_value = std::move(conv);
    valueChanged(m_value);
  }
  void setValue(ValueType value)
  {
    if(m_value == value)
      return;

    m_value = value;
    valueChanged(m_value);
  }
  void valueChanged(ValueType value) W_SIGNAL(valueChanged, value);

  W_PROPERTY(ValueType, value READ value NOTIFY valueChanged)

protected:
  ValueType m_value{};
};
W_OBJECT_IMPL(
    (GenericControlInlet<A, B, C>), template <typename A, typename B, typename C>)
struct SCORE_PLUGIN_JS_EXPORT MultiSlider
    : JS::GenericControlInlet<
          Process::MultiSlider, QVector<qreal>, std::vector<ossia::value>>
{
  W_OBJECT(MultiSlider);
  using GenericControlInlet::GenericControlInlet;
};
struct SCORE_PLUGIN_JS_EXPORT FileChooser
    : JS::GenericControlInlet<Process::FileChooser, QString, std::string>
{
  W_OBJECT(FileChooser);
  using GenericControlInlet::GenericControlInlet;
};
struct SCORE_PLUGIN_JS_EXPORT AudioFileChooser
    : JS::GenericControlInlet<Process::AudioFileChooser, QString, std::string>
{
  W_OBJECT(AudioFileChooser);
  using GenericControlInlet::GenericControlInlet;
};
struct SCORE_PLUGIN_JS_EXPORT VideoFileChooser
    : JS::GenericControlInlet<Process::VideoFileChooser, QString, std::string>
{
  W_OBJECT(VideoFileChooser);
  using GenericControlInlet::GenericControlInlet;
};

template <typename Impl = Process::FloatSlider>
class SCORE_PLUGIN_JS_EXPORT FloatSlider : public GenericControlInlet<Impl, float, float>
{
  W_OBJECT(FloatSlider)

public:
  using GenericControlInlet<Impl, float, float>::GenericControlInlet;
  virtual ~FloatSlider() override = default;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Impl{(float)m_min,       (float)m_max, (float)m_init,
                    this->objectName(), id,           parent};
  }

  W_INLINE_PROPERTY_VALUE(qreal, init, {0.5}, init, setInit, initChanged)
  W_INLINE_PROPERTY_VALUE(qreal, min, {0.}, getMin, setMin, minChanged)
  W_INLINE_PROPERTY_VALUE(qreal, max, {1.}, getMax, setMax, maxChanged)
};
W_OBJECT_IMPL(JS::FloatSlider<Impl>, template <typename Impl>)

template <typename Impl = Process::IntSlider>
class SCORE_PLUGIN_JS_EXPORT IntSlider : public GenericControlInlet<Impl, int, int>
{
  W_OBJECT(IntSlider)

public:
  using GenericControlInlet<Impl, int, int>::GenericControlInlet;
  virtual ~IntSlider() override = default;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Impl{m_min, m_max, m_init, this->objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(int, init, {0}, init, setInit, initChanged)
  W_INLINE_PROPERTY_VALUE(int, min, {0}, getMin, setMin, minChanged)
  W_INLINE_PROPERTY_VALUE(int, max, {127}, getMax, setMax, maxChanged)
};
W_OBJECT_IMPL(JS::IntSlider<Impl>, template <typename Impl>)

static constexpr auto vec2(double v)
{
  return QVector2D{(float)v, (float)v};
}
static constexpr auto vec3(double v)
{
  return QVector3D{(float)v, (float)v, (float)v};
}
static constexpr auto vec4(double v)
{
  return QVector4D{(float)v, (float)v, (float)v, (float)v};
}
static constexpr auto vec_to_ossia(QVector2D v)
{
  return ossia::vec2f{(float)v[0], (float)v[1]};
}
static constexpr auto vec_to_ossia(QVector3D v)
{
  return ossia::vec3f{(float)v[0], (float)v[1], (float)v[2]};
}
static constexpr auto vec_to_ossia(QVector4D v)
{
  return ossia::vec4f{(float)v[0], (float)v[1], (float)v[2], (float)v[3]};
}

template <typename Impl>
class SCORE_PLUGIN_JS_EXPORT FloatControl2D
    : public GenericControlInlet<Impl, QVector2D, ossia::vec2f>
{
  W_OBJECT(FloatControl2D)

public:
  using GenericControlInlet<Impl, QVector2D, ossia::vec2f>::GenericControlInlet;
  virtual ~FloatControl2D() override = default;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    if constexpr(std::is_constructible_v<
                     Impl, ossia::vec2f, ossia::vec2f, ossia::vec2f, bool, QString,
                     Id<Process::Port>, QObject*>)
      return new Impl{
          vec_to_ossia(m_min),
          vec_to_ossia(m_max),
          vec_to_ossia(m_init),
          false,
          this->objectName(),
          id,
          parent};
    else if constexpr(std::is_constructible_v<
                          Impl, ossia::vec2f, ossia::vec2f, ossia::vec2f, QString,
                          Id<Process::Port>, QObject*>)
      return new Impl{
          vec_to_ossia(m_min),
          vec_to_ossia(m_max),
          vec_to_ossia(m_init),
          this->objectName(),
          id,
          parent};
    else
      return new Impl{vec_to_ossia(m_init), this->objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(QVector2D, init, {vec2(0.)}, init, setInit, initChanged);
  W_INLINE_PROPERTY_VALUE(QVector2D, min, {vec2(0.)}, getMin, setMin, minChanged);
  W_INLINE_PROPERTY_VALUE(QVector2D, max, {vec2(1.)}, getMax, setMax, maxChanged);
};
W_OBJECT_IMPL(JS::FloatControl2D<Impl>, template <typename Impl>)

template <typename Impl>
class SCORE_PLUGIN_JS_EXPORT IntControl2D
    : public GenericControlInlet<Impl, QVector2D, ossia::vec2f>
{
  W_OBJECT(IntControl2D)

public:
  using GenericControlInlet<Impl, QVector2D, ossia::vec2f>::GenericControlInlet;
  virtual ~IntControl2D() override = default;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Impl{
        vec_to_ossia(m_min),
        vec_to_ossia(m_max),
        vec_to_ossia(m_init),
        true,
        this->objectName(),
        id,
        parent};
  }

  W_INLINE_PROPERTY_VALUE(QVector2D, init, {vec2(0.)}, init, setInit, initChanged);
  W_INLINE_PROPERTY_VALUE(QVector2D, min, {vec2(0.)}, getMin, setMin, minChanged);
  W_INLINE_PROPERTY_VALUE(QVector2D, max, {vec2(1.)}, getMax, setMax, maxChanged);
};
W_OBJECT_IMPL(JS::IntControl2D<Impl>, template <typename Impl>)

template <typename Impl>
class SCORE_PLUGIN_JS_EXPORT FloatControl1D_2D
    : public GenericControlInlet<Impl, QVector2D, ossia::vec2f>
{
  W_OBJECT(FloatControl1D_2D)

public:
  using GenericControlInlet<Impl, QVector2D, ossia::vec2f>::GenericControlInlet;
  virtual ~FloatControl1D_2D() override = default;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Impl(m_min, m_max, vec_to_ossia(m_init), this->objectName(), id, parent);
  }

  W_INLINE_PROPERTY_VALUE(QVector2D, init, {vec2(0.)}, init, setInit, initChanged);
  W_INLINE_PROPERTY_VALUE(float, min, {0.}, getMin, setMin, minChanged);
  W_INLINE_PROPERTY_VALUE(float, max, {1.}, getMax, setMax, maxChanged);
};
W_OBJECT_IMPL(JS::FloatControl1D_2D<Impl>, template <typename Impl>)

template <typename Impl>
class SCORE_PLUGIN_JS_EXPORT FloatControl3D
    : public GenericControlInlet<Impl, QVector3D, ossia::vec3f>
{
  W_OBJECT(FloatControl3D)

public:
  using GenericControlInlet<Impl, QVector3D, ossia::vec3f>::GenericControlInlet;
  virtual ~FloatControl3D() override = default;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    if constexpr(std::is_constructible_v<
                     Impl, ossia::vec3f, ossia::vec3f, ossia::vec3f, bool, QString,
                     Id<Process::Port>, QObject*>)
      return new Impl{
          vec_to_ossia(m_min),
          vec_to_ossia(m_max),
          vec_to_ossia(m_init),
          false,
          this->objectName(),
          id,
          parent};
    else if constexpr(std::is_constructible_v<
                          Impl, ossia::vec3f, ossia::vec3f, ossia::vec3f, QString,
                          Id<Process::Port>, QObject*>)
      return new Impl{
          vec_to_ossia(m_min),
          vec_to_ossia(m_max),
          vec_to_ossia(m_init),
          this->objectName(),
          id,
          parent};
    else
      return new Impl{vec_to_ossia(m_init), this->objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(QVector3D, init, {vec3(0.)}, init, setInit, initChanged);
  W_INLINE_PROPERTY_VALUE(QVector3D, min, {vec3(0.)}, getMin, setMin, minChanged);
  W_INLINE_PROPERTY_VALUE(QVector3D, max, {vec3(1.)}, getMax, setMax, maxChanged);
};
W_OBJECT_IMPL(JS::FloatControl3D<Impl>, template <typename Impl>)

template <typename Impl>
class SCORE_PLUGIN_JS_EXPORT IntControl3D
    : public GenericControlInlet<Impl, QVector3D, ossia::vec3f>
{
  W_OBJECT(IntControl3D)

public:
  using GenericControlInlet<Impl, QVector3D, ossia::vec3f>::GenericControlInlet;
  virtual ~IntControl3D() override = default;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Impl{
        vec_to_ossia(m_min),
        vec_to_ossia(m_max),
        vec_to_ossia(m_init),
        true,
        this->objectName(),
        id,
        parent};
  }

  W_INLINE_PROPERTY_VALUE(QVector3D, init, {vec3(0.)}, init, setInit, initChanged);
  W_INLINE_PROPERTY_VALUE(QVector3D, min, {vec3(0.)}, getMin, setMin, minChanged);
  W_INLINE_PROPERTY_VALUE(QVector3D, max, {vec3(1.)}, getMax, setMax, maxChanged);
};
W_OBJECT_IMPL(JS::IntControl3D<Impl>, template <typename Impl>)

template <typename Impl>
class SCORE_PLUGIN_JS_EXPORT FloatControl4D
    : public GenericControlInlet<Impl, QVector4D, ossia::vec4f>
{
  W_OBJECT(FloatControl4D)

public:
  using GenericControlInlet<Impl, QVector4D, ossia::vec4f>::GenericControlInlet;
  virtual ~FloatControl4D() override = default;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    if constexpr(std::is_constructible_v<
                     Impl, ossia::vec4f, ossia::vec4f, ossia::vec4f, QString,
                     Id<Process::Port>, QObject*>)
      return new Impl{
          vec_to_ossia(m_min),
          vec_to_ossia(m_max),
          vec_to_ossia(m_init),
          this->objectName(),
          id,
          parent};
    else
      return new Impl{vec_to_ossia(m_init), this->objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(QVector4D, init, {vec4(0.)}, init, setInit, initChanged);
  W_INLINE_PROPERTY_VALUE(QVector4D, min, {vec4(0.)}, getMin, setMin, minChanged);
  W_INLINE_PROPERTY_VALUE(QVector4D, max, {vec4(1.)}, getMax, setMax, maxChanged);
};
W_OBJECT_IMPL(JS::FloatControl4D<Impl>, template <typename Impl>)

class SCORE_PLUGIN_JS_EXPORT Enum : public ControlInlet
{
  W_OBJECT(Enum)

public:
  using ControlInlet::ControlInlet;
  virtual ~Enum() override;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::Enum{m_choices, {}, current(), objectName(), id, parent};
  }

  auto getValues() const { return choices(); }

  std::string current() const
  {
    if(!m_choices.isEmpty() && ossia::valid_index(m_index, m_choices))
    {
      return m_choices[m_index].toStdString();
    }
    return {};
  }

  W_INLINE_PROPERTY_VALUE(int, index, {}, index, setIndex, indexChanged)
  W_INLINE_PROPERTY_CREF(QStringList, choices, {}, choices, setChoices, choicesChanged)
};

class SCORE_PLUGIN_JS_EXPORT ComboBox : public ControlInlet
{
  W_OBJECT(ComboBox)

public:
  using ControlInlet::ControlInlet;
  virtual ~ComboBox() override;
  bool isEvent() const override { return true; }

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    std::vector<std::pair<QString, ossia::value>> choices;
    choices.reserve(m_choices.size());
    for(auto& c : m_choices)
      choices.emplace_back(c, c.toStdString());
    return new Process::ComboBox{choices, current(), objectName(), id, parent};
  }

  auto getValues() const { return choices(); }

  std::string current() const
  {
    if(!m_choices.isEmpty() && ossia::valid_index(m_index, m_choices))
    {
      return m_choices[m_index].toStdString();
    }
    return {};
  }

  W_INLINE_PROPERTY_VALUE(int, index, {}, index, setIndex, indexChanged)
  W_INLINE_PROPERTY_CREF(QStringList, choices, {}, choices, setChoices, choicesChanged)
};

class SCORE_PLUGIN_JS_EXPORT Toggle : public ControlInlet
{
  W_OBJECT(Toggle)

public:
  using ControlInlet::ControlInlet;
  virtual ~Toggle() override;
  bool isEvent() const override { return true; }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::Toggle{m_checked, objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(bool, checked, {}, checked, setChecked, checkedChanged)
};

class SCORE_PLUGIN_JS_EXPORT Button : public ControlInlet
{
  W_OBJECT(Button)

public:
  using ControlInlet::ControlInlet;
  virtual ~Button() override;
  bool isEvent() const override { return true; }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::Button{objectName(), id, parent};
  }

  W_INLINE_PROPERTY_VALUE(bool, checked, {}, checked, setChecked, checkedChanged)
};

class SCORE_PLUGIN_JS_EXPORT Impulse : public ControlInlet
{
  W_OBJECT(Impulse)

public:
  using ControlInlet::ControlInlet;
  virtual ~Impulse() override;
  bool isEvent() const override { return false; }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::ImpulseButton{objectName(), id, parent};
  }

  void impulse() W_SIGNAL(impulse);
};

class SCORE_PLUGIN_JS_EXPORT LineEdit : public ControlInlet
{
  W_OBJECT(LineEdit)

public:
  using ControlInlet::ControlInlet;
  virtual ~LineEdit() override;
  bool isEvent() const override { return true; }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::LineEdit{m_text, objectName(), id, parent};
  }

  W_INLINE_PROPERTY_CREF(QString, text, {}, text, setText, textChanged)
};

class SCORE_PLUGIN_JS_EXPORT HSVSlider
    : public GenericControlInlet<Process::HSVSlider, QColor, ossia::vec4f>
{
  W_OBJECT(HSVSlider)

public:
  using GenericControlInlet<
      Process::HSVSlider, QColor, ossia::vec4f>::GenericControlInlet;
  virtual ~HSVSlider() override;
  bool isEvent() const override { return true; }
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto col = init().toRgb();
    ossia::vec4f v{
        (float)col.redF(), (float)col.greenF(), (float)col.blueF(), (float)col.alphaF()};
    return new Process::HSVSlider{v, objectName(), id, parent};
  }

  void setValue(QVariant value) override
  {
    auto vec = value.value<QVector4D>();
    auto conv = QColor::fromRgbF(vec[0], vec[1], vec[2], vec[3]);
    if(m_value == conv)
      return;

    m_value = std::move(conv);
    valueChanged(m_value);
  }
  W_INLINE_PROPERTY_CREF(QColor, init, {}, init, setInit, initChanged)
};
class SCORE_PLUGIN_JS_EXPORT ValueOutlet : public Outlet
{
  W_OBJECT(ValueOutlet)

  QJSValue m_value;

public:
  std::vector<OutValueMessage> values;

  explicit ValueOutlet(QObject* parent = nullptr);
  virtual ~ValueOutlet() override;
  const QJSValue& value() const;
  void clear()
  {
    m_value = QJSValue{};
    values.clear();
  }
  Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::ValueOutlet(objectName(), id, parent);
  }

public:
  void setValue(const QJSValue& value);
  W_SLOT(setValue);
  void addValue(qreal timestamp, QJSValue t);
  W_SLOT(addValue);

  W_PROPERTY(QJSValue, value READ value WRITE setValue)
};

class SCORE_PLUGIN_JS_EXPORT AudioInlet : public Inlet
{
  W_OBJECT(AudioInlet)

public:
  explicit AudioInlet(QObject* parent = nullptr);
  virtual ~AudioInlet() override;
  const QVector<QVector<double>>& audio() const;
  void setAudio(const QVector<QVector<double>>& audio);

  QVector<double> channel(int i) const
  {
    if(m_audio.size() > i)
      return m_audio[i];
    return {};
  }
  W_INVOKABLE(channel);

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::AudioInlet(objectName(), id, parent);
  }

private:
  QVector<QVector<double>> m_audio;
};

class SCORE_PLUGIN_JS_EXPORT AudioOutlet : public Outlet
{
  W_OBJECT(AudioOutlet)

public:
  explicit AudioOutlet(QObject* parent = nullptr);
  virtual ~AudioOutlet() override;
  Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Process::AudioOutlet(objectName(), id, parent);
    if(id.val() == 0)
      p->setPropagate(true);
    return p;
  }

  const QVector<QVector<double>>& audio() const;

  void setChannel(int i, const QJSValue& v);
  W_INVOKABLE(setChannel)
private:
  QVector<QVector<double>> m_audio;
};

class SCORE_PLUGIN_JS_EXPORT MidiMessage
{
  W_GADGET(MidiMessage)

public:
  QByteArray bytes;

  W_PROPERTY(QByteArray, bytes MEMBER bytes)
};

class SCORE_PLUGIN_JS_EXPORT MidiInlet : public Inlet
{
  W_OBJECT(MidiInlet)

public:
  explicit MidiInlet(QObject* parent = nullptr);
  virtual ~MidiInlet() override;
  template <typename T>
  void setMidi(const T& arr)
  {
    m_midi.clear();
    for(const libremidi::ump& mess : arr)
    {
      auto m1 = libremidi::midi1_from_ump(mess);
      const auto N = mess.size();
      QVector<int> m;
      m.resize(N);

      for(std::size_t i = 0; i < N; i++)
        m[i] = m1.bytes[i];

      m_midi.push_back(QVariant::fromValue(m));
    }
  }

  QVariantList messages() const { return m_midi; }
  W_INVOKABLE(messages);

  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::MidiInlet(objectName(), id, parent);
  }

private:
  QVariantList m_midi;
};

class SCORE_PLUGIN_JS_EXPORT MidiOutlet : public Outlet
{
  W_OBJECT(MidiOutlet)

public:
  explicit MidiOutlet(QObject* parent = nullptr);
  virtual ~MidiOutlet() override;
  Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    return new Process::MidiOutlet(objectName(), id, parent);
  }

  void clear();
  const QVector<QVector<int>>& midi() const;

  void setMessages(const QVariantList m)
  {
    m_midi.clear();
    for(auto& v : m)
    {
      if(v.canConvert<QVector<int>>())
        m_midi.push_back(v.value<QVector<int>>());
    }
  }
  W_INVOKABLE(setMessages);

  void add(QVector<int> m) { m_midi.push_back(std::move(m)); }
  W_INVOKABLE(add);

private:
  QVector<QVector<int>> m_midi;
};

#if defined(SCORE_HAS_GPU_JS)
class TextureInlet : public Inlet
{
  W_OBJECT(TextureInlet)

public:
  explicit TextureInlet(QObject* parent = nullptr);
  virtual ~TextureInlet() override;
  Process::Inlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Gfx::TextureInlet(objectName(), id, parent);
    return p;
  }

  QQuickItem* item() const noexcept { return m_item; }

  W_PROPERTY(QQuickItem*, item READ item CONSTANT)
private:
  QQuickItem* m_item{};
};

class TextureOutlet : public Outlet
{
  W_OBJECT(TextureOutlet)

public:
  explicit TextureOutlet(QObject* parent = nullptr);
  virtual ~TextureOutlet() override;
  Process::Outlet* make(Id<Process::Port>&& id, QObject* parent) override
  {
    auto p = new Gfx::TextureOutlet(objectName(), id, parent);
    return p;
  }

  QQuickItem* item() const noexcept { return m_item; }
  void setItem(QQuickItem* v) { m_item = v; }

  W_PROPERTY(QQuickItem*, item READ item WRITE setItem CONSTANT)
private:
  QQuickItem* m_item{};
};
#endif

class Script : public QObject
{
  W_OBJECT(Script)
  W_CLASSINFO("DefaultProperty", "data")
  W_CLASSINFO(
      "qt_QmlJSWrapperFactoryMethod", "_q_createJSWrapper(QV4::ExecutionEngine*)")

public:
  QQmlListProperty<QObject> data() noexcept { return {this, &m_data}; }

  QJSValue& tick() /*Qt6: const*/ noexcept { return m_tick; }
  void setTick(const QJSValue& v) { m_tick = v; }
  QJSValue& start() /*Qt6: const*/ noexcept { return m_start; }
  void setStart(const QJSValue& v) { m_start = v; }
  QJSValue& stop() /*Qt6: const*/ noexcept { return m_stop; }
  void setStop(const QJSValue& v) { m_stop = v; }
  QJSValue& pause() /*Qt6: const*/ noexcept { return m_pause; }
  void setPause(const QJSValue& v) { m_pause = v; }
  QJSValue& resume() /*Qt6: const*/ noexcept { return m_resume; }
  void setResume(const QJSValue& v) { m_resume = v; }
  W_PROPERTY(QJSValue, tick READ tick WRITE setTick CONSTANT)
  W_PROPERTY(QJSValue, start READ start WRITE setStart CONSTANT)
  W_PROPERTY(QJSValue, stop READ stop WRITE setStop CONSTANT)
  W_PROPERTY(QJSValue, pause READ pause WRITE setPause CONSTANT)
  W_PROPERTY(QJSValue, resume READ resume WRITE setResume CONSTANT)
  W_PROPERTY(QQmlListProperty<QObject>, data READ data)

private:
  QList<QObject*> m_data;
  QJSValue m_tick;
  QJSValue m_start;
  QJSValue m_stop;
  QJSValue m_pause;
  QJSValue m_resume;
};
}

inline QDataStream& operator<<(QDataStream& i, const JS::MidiMessage& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator>>(QDataStream& i, JS::MidiMessage& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator<<(QDataStream& i, const JS::InValueMessage& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator>>(QDataStream& i, JS::InValueMessage& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator<<(QDataStream& i, const JS::OutValueMessage& sel)
{
  SCORE_ABORT;
  return i;
}
inline QDataStream& operator>>(QDataStream& i, JS::OutValueMessage& sel)
{
  SCORE_ABORT;
  return i;
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
