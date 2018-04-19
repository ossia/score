#pragma once
#include <QLineEdit>
#include <State/Widgets/AddressValidator.hpp>
namespace State
{

class SCORE_LIB_STATE_EXPORT AddressFragmentLineEdit final : public QLineEdit
{
public:
  AddressFragmentLineEdit(QWidget* parent) : QLineEdit{parent}
  {
    setValidator(new AddressFragmentValidator{this});
  }

  virtual ~AddressFragmentLineEdit();
};
}
