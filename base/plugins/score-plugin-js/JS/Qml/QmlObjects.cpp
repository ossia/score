#include "QmlObjects.hpp"

namespace JS
{

ValueInlet::ValueInlet(QObject* parent): QObject{parent} {}

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

ValueOutlet::ValueOutlet(QObject* parent): QObject{parent} {}

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

}
