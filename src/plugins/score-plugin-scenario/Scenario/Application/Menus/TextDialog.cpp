// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TextDialog.hpp"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QTextEdit>

class QWidget;
namespace Scenario
{
TextDialog::TextDialog(const QString& s, QWidget* parent) : QDialog{parent}
{
  this->setLayout(new QGridLayout);
  auto textEdit = new QTextEdit;
  textEdit->setPlainText(s);
  layout()->addWidget(textEdit);
  auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
  layout()->addWidget(buttonBox);

  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
}
