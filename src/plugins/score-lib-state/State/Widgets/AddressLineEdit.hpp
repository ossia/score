#pragma once
#include "AddressValidator.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressValidator.hpp>

#include <QDropEvent>
#include <QLineEdit>
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
    connect(this, &QLineEdit::textChanged, this, [&](const QString& str) {
      QString s = str;
      int i = 0;
#ifndef QT_NO_STYLE_STYLESHEET
      if (m_validator.validate(s, i) == QValidator::State::Acceptable)
      {
        this->setStyleSheet("QLineEdit { background: black; }");
      }
      else
      {
        this->setStyleSheet("QLineEdit { background: #660000; }");
      }
#endif
    });
  }

private:
  void dragEnterEvent(QDragEnterEvent* ev) override
  {
    static_cast<Parent_T*>(parent())->dragEnterEvent(ev);
  }
  void dropEvent(QDropEvent* ev) override
  {
    static_cast<Parent_T*>(parent())->dropEvent(ev);
  }

  Validator_T m_validator;
};

template <typename Parent_T>
class AddressLineEdit final
    : public AddressLineEditBase<AddressValidator, Parent_T>
{
public:
  using AddressLineEditBase<AddressValidator, Parent_T>::AddressLineEditBase;
};

template <typename Parent_T>
class AddressAccessorLineEdit final
    : public AddressLineEditBase<AddressAccessorValidator, Parent_T>
{
public:
  using AddressLineEditBase<AddressAccessorValidator, Parent_T>::
      AddressLineEditBase;
};
}
