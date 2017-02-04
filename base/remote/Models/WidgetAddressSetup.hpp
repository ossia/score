#pragma once
#include <QQmlProperty>
#include <Models/GUIItem.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <ossia/network/domain/domain.hpp>
#include <QQuickItem>


namespace RemoteUI
{
struct SetSliderAddress
{
  GUIItem& item;
  const Device::FullAddressSettings& address;

  void operator()(State::impulse c)
  {
    // Do nothing
    item.m_connection = QObject::connect(item.item(), SIGNAL(clicked()),
                     &item, SLOT(on_impulse()));
  }
  void operator()(bool c)
  {
    QQmlProperty(item.item(), "slider.from").write(0.);
    QQmlProperty(item.item(), "slider.to").write(1.);
    QQmlProperty(item.item(), "slider.stepSize").write(1);
    QQmlProperty(item.item(), "slider.value").write((qreal)c);
    item.m_connection = QObject::connect(item.item(), SIGNAL(toggled()),
                     &item, SLOT(on_impulse()));
  }

  void operator()(int i)
  {
    auto min = ossia::convert<int>(ossia::net::get_min(address.domain.get()));
    auto max = ossia::convert<int>(ossia::net::get_max(address.domain.get()));

    QQmlProperty(item.item(), "slider.from").write((qreal)min);
    QQmlProperty(item.item(), "slider.to").write((qreal)max);
    QQmlProperty(item.item(), "slider.stepSize").write(1);
    QQmlProperty(item.item(), "slider.value").write((qreal)i);

    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(qreal)),
                     &item, SLOT(on_intValueChanged(qreal)));
  }

  void operator()(float f)
  {
    auto min = ossia::convert<float>(ossia::net::get_min(address.domain.get()));
    auto max = ossia::convert<float>(ossia::net::get_max(address.domain.get()));

    QQmlProperty(item.item(), "slider.from").write((qreal)min);
    QQmlProperty(item.item(), "slider.to").write((qreal)max);
    QQmlProperty(item.item(), "slider.value").write((qreal)f);

    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(qreal)),
                     &item, SLOT(on_floatValueChanged(qreal)));
  }

  void operator()(char c)
  {
    QQmlProperty(item.item(), "min_chars").write(1);
    QQmlProperty(item.item(), "max_chars").write(1);
    QQmlProperty(item.item(), "value").write(c);

  }
  void operator()(const std::string& s)
  {
    QQmlProperty(item.item(), "value").write(QString::fromStdString(s));
  }

  template<std::size_t N>
  void operator()(std::array<float, N> c)
  {
    // TODO
  }

  void operator()(const State::tuple_t& c)
  {
    // TODO
  }
};



struct SetCheckboxAddress
{
  GUIItem& item;
  const Device::FullAddressSettings& address;

  void operator()(State::impulse)
  {
    // Do nothing
    item.m_connection = QObject::connect(item.item(), SIGNAL(toggled()),
                     &item, SLOT(on_impulse()));
  }
  void operator()(bool)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(bool)),
                     &item, SLOT(on_boolValueChanged(bool)));
  }

  void operator()(int i)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(bool)),
                     &item, SLOT(on_boolValueChanged(bool)));
  }

  void operator()(float f)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(bool)),
                     &item, SLOT(on_boolValueChanged(bool)));
  }

  void operator()(char c)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(bool)),
                     &item, SLOT(on_boolValueChanged(bool)));
  }
  void operator()(const std::string& s)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(valueChange(bool)),
                     &item, SLOT(on_boolValueChanged(bool)));
  }

  template<std::size_t N>
  void operator()(std::array<float, N> c)
  {
    // TODO
  }

  void operator()(const State::tuple_t& c)
  {
    // TODO
  }
};


struct SetLineEditAddress
{
  GUIItem& item;
  const Device::FullAddressSettings& address;

  void operator()(State::impulse)
  {
    // Do nothing
    item.m_connection = QObject::connect(item.item(), SIGNAL(toggled()),
                     &item, SLOT(on_impulse()));
  }
  void operator()(bool b)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(textChange(QString)),
                     &item, SLOT(on_parsableValueChanged(QString)));
  }

  void operator()(int i)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(textChange(QString)),
                     &item, SLOT(on_parsableValueChanged(QString)));
  }

  void operator()(float f)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(textChange(QString)),
                     &item, SLOT(on_parsableValueChanged(QString)));
  }

  void operator()(char c)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(textChange(QString)),
                     &item, SLOT(on_parsableValueChanged(QString)));
  }
  void operator()(const std::string& s)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(textChange(QString)),
                     &item, SLOT(on_stringValueChanged(QString)));
  }

  template<std::size_t N>
  void operator()(std::array<float, N> c)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(textChange(QString)),
                     &item, SLOT(on_parsableValueChanged(QString)));
  }

  void operator()(const State::tuple_t& c)
  {
    item.m_connection = QObject::connect(item.item(), SIGNAL(textChange(QString)),
                     &item, SLOT(on_parsableValueChanged(QString)));
  }
};


struct SetLabelAddress
{
  GUIItem& item;
  const Device::FullAddressSettings& address;

  void operator()(State::impulse)
  {
    // Do nothing
    QQmlProperty(item.item(), "text.text")
        .write("Impulse");
  }
  void operator()(bool b)
  {
    QQmlProperty(item.item(), "text.text")
        .write(QString("Bool: ") + (b ? "true" : "false"));
  }

  void operator()(int i)
  {
    QQmlProperty(item.item(), "text.text")
        .write(QString("Int: ") + QString::number(i));
  }

  void operator()(float f)
  {
    QQmlProperty(item.item(), "text.text")
        .write(QString("Float: ") + QString::number(f));
  }

  void operator()(char c)
  {
    QQmlProperty(item.item(), "text.text")
        .write(QString("Char: ") + QChar(c));
  }
  void operator()(const std::string& s)
  {
    QQmlProperty(item.item(), "text.text")
        .write("String: " + QString::fromStdString(s));
  }

  template<std::size_t N>
  void operator()(std::array<float, N> c)
  {
    QQmlProperty(item.item(), "text.text")
        .write("Array" + QString::fromStdString(ossia::convert<std::string>(c)));
  }

  void operator()(const State::tuple_t& c)
  {
    QQmlProperty(item.item(), "text.text")
        .write("Tuple" + State::convert::value<QString>(State::Value::fromValue(c)));
  }
};

}
