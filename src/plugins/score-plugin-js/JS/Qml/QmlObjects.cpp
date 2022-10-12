#include "QmlObjects.hpp"

#include <JS/Qml/Metatypes.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::Inlet)
W_OBJECT_IMPL(JS::Outlet)
W_OBJECT_IMPL(JS::ValueInlet)
W_OBJECT_IMPL(JS::ValueOutlet)
W_OBJECT_IMPL(JS::ControlInlet)
W_OBJECT_IMPL(JS::MidiInlet)
W_OBJECT_IMPL(JS::MidiOutlet)
W_OBJECT_IMPL(JS::AudioInlet)
W_OBJECT_IMPL(JS::AudioOutlet)
W_OBJECT_IMPL(JS::FloatSlider)
W_OBJECT_IMPL(JS::IntSlider)
W_OBJECT_IMPL(JS::Enum)
W_OBJECT_IMPL(JS::Toggle)
W_OBJECT_IMPL(JS::Button)
W_OBJECT_IMPL(JS::Impulse)
W_OBJECT_IMPL(JS::LineEdit)

W_GADGET_IMPL(JS::InValueMessage)
W_GADGET_IMPL(JS::OutValueMessage)
W_GADGET_IMPL(JS::MidiMessage)

auto disregard_me = &JS::InValueMessage::staticMetaObject;
auto disregard_me_1 = &JS::OutValueMessage::staticMetaObject;
auto disregard_me_2 = &JS::MidiMessage::staticMetaObject;
namespace JS
{

ValueInlet::ValueInlet(QObject* parent)
    : Inlet{parent}
{
}

ValueInlet::~ValueInlet() { }

QVariant ValueInlet::value() const
{
  return m_value;
}

void ValueInlet::setValue(QVariant value)
{
  if(m_value == value)
    return;

  m_value = value;
  valueChanged(value);
}

ControlInlet::ControlInlet(QObject* parent)
    : Inlet{parent}
{
}

ControlInlet::~ControlInlet() { }

QVariant ControlInlet::value() const
{
  return m_value;
}

void ControlInlet::setValue(QVariant value)
{
  if(m_value == value)
    return;

  m_value = value;
  valueChanged(m_value);
}

ValueOutlet::ValueOutlet(QObject* parent)
    : Outlet{parent}
{
}

ValueOutlet::~ValueOutlet() { }

const QJSValue& ValueOutlet::value() const
{
  return m_value;
}

void ValueOutlet::setValue(const QJSValue& value)
{
  m_value = value;
}

void ValueOutlet::addValue(qreal timestamp, QJSValue t)
{
  values.push_back({timestamp, std::move(t)});
}

AudioInlet::AudioInlet(QObject* parent)
    : Inlet{parent}
{
}

AudioInlet::~AudioInlet() { }

const QVector<QVector<double>>& AudioInlet::audio() const
{
  return m_audio;
}

void AudioInlet::setAudio(const QVector<QVector<double>>& audio)
{
  m_audio = audio;
}

AudioOutlet::AudioOutlet(QObject* parent)
    : Outlet{parent}
{
}

AudioOutlet::~AudioOutlet() { }

const QVector<QVector<double>>& AudioOutlet::audio() const
{
  return m_audio;
}

MidiInlet::MidiInlet(QObject* parent)
    : Inlet{parent}
{
}

MidiInlet::~MidiInlet() { }

MidiOutlet::MidiOutlet(QObject* parent)
    : Outlet{parent}
{
}

MidiOutlet::~MidiOutlet() { }

void MidiOutlet::clear()
{
  m_midi.clear();
}

const QVector<QVector<int>>& MidiOutlet::midi() const
{
  return m_midi;
}

Inlet::~Inlet() { }

Outlet::~Outlet() { }

FloatSlider::~FloatSlider() = default;
IntSlider::~IntSlider() = default;
Toggle::~Toggle() = default;
Button::~Button() = default;
Impulse::~Impulse() = default;
Enum::~Enum() = default;
LineEdit::~LineEdit() = default;
}
