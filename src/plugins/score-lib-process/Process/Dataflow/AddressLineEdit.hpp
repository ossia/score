#pragma once
#include <functional>
#include <optional>
#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressValidator.hpp>

#include <score/model/Skin.hpp>
#include <score/widgets/ValidationPalette.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <QDropEvent>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPalette>
#include <QValidator>

namespace Process
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
  explicit AddressLineEditBase(Device::NodeBasedItemModel* model, QWidget* parent)
      : QLineEdit{parent}
      , m_model{model}
  {
    setAcceptDrops(true);
    setMinimumHeight(24);
    connect(this, &QLineEdit::textChanged, this, &AddressLineEditBase::updatePalette);
  }

  //! Whether a local device address is published; nullopt for other devices
  void setLocalCheck(std::function<std::optional<bool>(const State::Address&)> f)
  {
    m_local = std::move(f);
    updatePalette(this->text());
  }

  void updatePalette(const QString& str)
  {
    QString s = str;
    int i = 0;
    auto validity = score::InputValidity::Valid;
    if(m_validator.validate(s, i) == QValidator::State::Acceptable)
    {
      auto addr = State::parseAddressAccessor(s);
      std::optional<bool> local;
      if(addr && m_local)
        local = m_local(addr->address);

      if(local)
      {
        if(!*local)
          validity = score::InputValidity::Invalid;
      }
      else if(m_model && addr)
      {
        // Look into the tree to see if the node actually exists
        if(!Device::try_getNodeFromAddress(m_model->rootNode(), addr->address))
          validity = score::InputValidity::Unknown;
      }
    }
    else
    {
      validity = score::InputValidity::Invalid;
    }
    score::setInputValidity(*this, validity);
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
  Device::NodeBasedItemModel* m_model{};
  std::function<std::optional<bool>(const State::Address&)> m_local;
};

template <typename Parent_T>
class AddressLineEdit final
    : public AddressLineEditBase<State::AddressValidator, Parent_T>
{
public:
  using AddressLineEditBase<State::AddressValidator, Parent_T>::AddressLineEditBase;
};

template <typename Parent_T>
class AddressAccessorLineEdit final
    : public AddressLineEditBase<State::AddressAccessorValidator, Parent_T>
{
public:
  using AddressLineEditBase<
      State::AddressAccessorValidator, Parent_T>::AddressLineEditBase;
};
}
