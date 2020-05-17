#pragma once
#include "AddressValidator.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressValidator.hpp>

#include <QDropEvent>
#include <QLineEdit>
#include <QPalette>
#include <QValidator>

namespace State
{
/**
 * @brief The AddressLineEdit class
 *
 * Used to input an address. Changes colors to red-ish if it is invalid.
 */
template <class Validator_T, class Parent_T>
class AddressLineEditBase : public QLineEdit
{
public:
  explicit AddressLineEditBase(Parent_T* parent) : QLineEdit{parent}
  {
    setAcceptDrops(true);
    setMinimumHeight(24);
    connect(this, &QLineEdit::textChanged, this, [&](const QString& str) {
      QString s = str;
      int i = 0;
      QPalette palette{this->palette()};
      if (m_validator.validate(s, i) == QValidator::State::Acceptable)
      {
        palette.setColor(QPalette::Base, QColor{"#161514"});
        palette.setColor(QPalette::Light, QColor{"#c58014"});
        palette.setColor(QPalette::Midlight, QColor{"#161514"});
      }
      else
      {
        palette.setColor(QPalette::Base, QColor{"#300000"});
        palette.setColor(QPalette::Light, QColor{"#660000"});
        palette.setColor(QPalette::Midlight, QColor{"#500000"});
      }
      this->setPalette(palette);
    });
  }

private:
  void dragEnterEvent(QDragEnterEvent* ev) override
  {
    static_cast<Parent_T*>(parent())->dragEnterEvent(ev);
  }
  void dropEvent(QDropEvent* ev) override { static_cast<Parent_T*>(parent())->dropEvent(ev); }

  Validator_T m_validator;
};

template <typename Parent_T>
class AddressLineEdit final : public AddressLineEditBase<AddressValidator, Parent_T>
{
public:
  using AddressLineEditBase<AddressValidator, Parent_T>::AddressLineEditBase;
};

template <typename Parent_T>
class AddressAccessorLineEdit final
    : public AddressLineEditBase<AddressAccessorValidator, Parent_T>
{
public:
  using AddressLineEditBase<AddressAccessorValidator, Parent_T>::AddressLineEditBase;
};
}
