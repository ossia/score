#include "QmlObjects.hpp"

namespace JS
{

ValueInlet::ValueInlet(QObject* parent): Inlet{parent} {}

ValueInlet::~ValueInlet() {}

QVariant ValueInlet::value() const
{
  return m_value;
}

void ValueInlet::setValue(QVariant value)
{
  if (m_value == value)
    return;

  m_value = value;
  emit valueChanged(m_value);
}

ValueOutlet::ValueOutlet(QObject* parent): Outlet{parent} {}

ValueOutlet::~ValueOutlet() {}

QVariant ValueOutlet::value() const
{
  return m_value;
}

void ValueOutlet::setValue(QVariant value)
{
  if (m_value == value)
    return;

  m_value = value;
  emit valueChanged(m_value);
}

AudioInlet::AudioInlet(QObject* parent): Inlet{parent} {}

AudioInlet::~AudioInlet() { }

const QVector<QVector<double> >&AudioInlet::audio() const
{ return m_audio; }

void AudioInlet::setAudio(const QVector<QVector<double> >& audio)
{
  m_audio = audio;
}

AudioOutlet::AudioOutlet(QObject* parent): Outlet{parent} {}

AudioOutlet::~AudioOutlet() { }

const QVector<QVector<double> >&AudioOutlet::audio() const
{ return m_audio; }


MidiInlet::MidiInlet(QObject* parent): Inlet{parent} {}

MidiInlet::~MidiInlet() { }




MidiOutlet::MidiOutlet(QObject* parent): Outlet{parent} {}

MidiOutlet::~MidiOutlet() { }

const QVector<MidiMessage>& MidiOutlet::midi() const
{ return m_midi; }


Inlet::~Inlet()
{

}

Outlet::~Outlet()
{

}

FloatSlider::~FloatSlider() = default;
IntSlider::~IntSlider() = default;
Toggle::~Toggle() = default;
Enum::~Enum() = default;
LineEdit::~LineEdit() = default;

}
