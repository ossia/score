// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressFragmentLineEdit.hpp"

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
    QPalette palette{this->palette()};
    if (validator()->validate(s, i) == QValidator::State::Acceptable)
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
}
