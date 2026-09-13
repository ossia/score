// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressFragmentLineEdit.hpp"

#include <score/model/Skin.hpp>

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
    auto& skin = score::Skin::instance();
    QPalette palette{qApp->palette()};
    if(validator()->validate(s, i) == QValidator::State::Acceptable)
    {
      // Valid: the application palette, untinted.
    }
    else
    {
      palette.setColor(QPalette::Base, skin.Warn3.darker.brush.color());
      palette.setColor(QPalette::Light, skin.Warn3.color());
      palette.setColor(QPalette::Midlight, skin.Warn3.darker300.brush.color());
    }
    this->setPalette(palette);
  });
}
}
