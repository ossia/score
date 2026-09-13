#pragma once
#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressValidator.hpp>

#include <score/model/Skin.hpp>

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

  void updatePalette(const QString& str)
  {
    QString s = str;
    int i = 0;
    // Starts from the application palette, not this widget's: it may still
    // carry the tint from the last time the text was wrong.
    auto& skin = score::Skin::instance();
    QPalette palette{qApp->palette()};
    if(m_validator.validate(s, i) == QValidator::State::Acceptable)
    {
      if(m_model)
      {
        // Look into the tree to see if the node actually exists
        auto addr = State::parseAddressAccessor(s);

        if(Device::try_getNodeFromAddress(m_model->rootNode(), addr->address))
        {
          // Valid and present: the application palette, untinted.
        }
        else
        {
          palette.setColor(QPalette::Base, skin.Warn2.darker.brush.color());
          palette.setColor(QPalette::Light, skin.Warn3.color());
          palette.setColor(QPalette::Midlight, skin.Warn3.darker.brush.color());
        }
      }
      else
      {
      }
    }
    else
    {
      palette.setColor(QPalette::Base, skin.Warn3.darker.brush.color());
      palette.setColor(QPalette::Light, skin.Warn3.color());
      palette.setColor(QPalette::Midlight, skin.Warn3.darker300.brush.color());
    }
    this->setPalette(palette);
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
