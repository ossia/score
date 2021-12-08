#pragma once
#include <State/Widgets/AddressValidator.hpp>

#include <QLineEdit>
namespace State
{

class SCORE_LIB_STATE_EXPORT AddressFragmentLineEdit final : public QLineEdit
{
public:
  explicit AddressFragmentLineEdit(QWidget* parent);
  virtual ~AddressFragmentLineEdit();
};

}
