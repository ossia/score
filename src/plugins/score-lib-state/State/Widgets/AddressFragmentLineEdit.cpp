// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressFragmentLineEdit.hpp"

#include <score/widgets/ValidationPalette.hpp>

#include <QGuiApplication>

namespace State
{
AddressFragmentLineEdit::~AddressFragmentLineEdit() = default;

AddressFragmentLineEdit::AddressFragmentLineEdit(QWidget* parent)
    : QLineEdit{parent}
{
  setValidator(new AddressFragmentValidator{this});

  connect(this, &QLineEdit::textChanged, this, [&](const QString& str) {
    if(!this->validator())
      return;

    QString s = str;
    int i = 0;
    score::setInputValidity(
        *this, validator()->validate(s, i) == QValidator::State::Acceptable
                   ? score::InputValidity::Valid
                   : score::InputValidity::Invalid);
  });
}
}
