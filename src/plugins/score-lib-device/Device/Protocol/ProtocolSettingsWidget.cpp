// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProtocolSettingsWidget.hpp"

#include <QCheckBox>
#include <QCodeEditor>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Device::ProtocolSettingsWidget)
namespace Device
{
ProtocolSettingsWidget::~ProtocolSettingsWidget() = default;
AddressDialog::~AddressDialog() = default;

void ProtocolSettingsWidget::checkForChanges(QLineEdit* w)
{
  connect(w, &QLineEdit::textEdited, this, &ProtocolSettingsWidget::changed);
}
void ProtocolSettingsWidget::checkForChanges(QComboBox* w)
{
  connect(
      w, qOverload<int>(&QComboBox::currentIndexChanged), this,
      &ProtocolSettingsWidget::changed);
}
void ProtocolSettingsWidget::checkForChanges(QSpinBox* w)
{
  connect(
      w, qOverload<int>(&QSpinBox::valueChanged), this,
      &ProtocolSettingsWidget::changed);
}
void ProtocolSettingsWidget::checkForChanges(QTextEdit* w)
{
  if(auto edit = qobject_cast<QCodeEditor*>(w))
    connect(edit, &QCodeEditor::editingFinished, this, &ProtocolSettingsWidget::changed);
}
void ProtocolSettingsWidget::checkForChanges(QCheckBox* w)
{
  connect(w, &QCheckBox::stateChanged, this, &ProtocolSettingsWidget::changed);
}
Device::Node ProtocolSettingsWidget::getDevice() const
{
  return Device::Node{getSettings(), nullptr};
}

}
