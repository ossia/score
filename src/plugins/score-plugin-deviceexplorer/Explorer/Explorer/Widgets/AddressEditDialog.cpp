// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressEditDialog.hpp"

#include <State/Domain.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <State/Widgets/AddressValidator.hpp>
#include <State/Widgets/Values/TypeComboBox.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>

#include <Explorer/Common/AddressSettings/AddressSettingsFactory.hpp>
#include <Explorer/Common/AddressSettings/Widgets/AddressSettingsWidget.hpp>

#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/WidgetWrapper.hpp>

#include <ossia/network/base/name_validation.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QString>
#include <qnamespace.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Explorer::AddressEditDialog)

namespace Explorer
{
class AddressBraceExpressionValidator : public QValidator
{
public:
  using QValidator::QValidator;
  virtual ~AddressBraceExpressionValidator() { }

  QValidator::State validate(QString& s, int& pos) const override
  {
    if(::State::Address::validateFragment(s))
      return QValidator::State::Acceptable;
    QString str = s;
    str.remove('/');

    if(::State::Address::validateFragment(str))
      return QValidator::State::Acceptable;

    if(str.contains('[') || str.contains(']') || str.contains('{') || str.contains('}'))
    {
      if(ossia::net::is_brace_expansion(str.toStdString()))
        return QValidator::State::Acceptable;
      else
        return QValidator::State::Intermediate;
    }

    return QValidator::State::Invalid;
  }
};
AddressEditDialog::AddressEditDialog(QWidget* parent)
    : AddressEditDialog{makeDefaultSettings(), parent}
{
  m_nameEdit->setValidator(new AddressBraceExpressionValidator);
}

AddressEditDialog::AddressEditDialog(
    const Device::AddressSettings& addr, QWidget* parent)
    : Device::AddressDialog{parent}
    , m_originalSettings{addr}
{
  this->setMinimumWidth(500);
  m_layout = new QFormLayout;
  setLayout(m_layout);

  // Name
  m_nameEdit = new State::AddressFragmentLineEdit{this};
  m_layout->addRow(tr("Name"), m_nameEdit);

  setNodeSettings();

  // Value type
  auto typeCb = new State::TypeComboBox{this};

  connect(
      typeCb, &State::TypeComboBox::changed, this, &AddressEditDialog::updateType,
      Qt::QueuedConnection);

  m_valueTypeCBox = typeCb;

  m_layout->addRow(tr("Value type"), m_valueTypeCBox);

  // AddressWidget
  m_addressWidget = new WidgetWrapper<AddressSettingsWidget>{this};
  m_layout->addRow(m_addressWidget);

  setValueSettings();

  // Ok / Cancel
  auto buttonBox = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this};
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  m_layout->addWidget(buttonBox);
}

AddressEditDialog::~AddressEditDialog() { }

void AddressEditDialog::updateType(ossia::val_type valueType)
{
  auto widg = AddressSettingsFactory{}(valueType);

  m_addressWidget->setWidget(widg);

  if(!m_originalSettings.ioType)
    m_originalSettings.ioType = ossia::access_mode::BI;

  if(widg)
  {
    auto& dom = m_originalSettings.domain.get();
    if(!dom)
      dom = widg->getDefaultSettings().domain.get();
    widg->setSettings(m_originalSettings);
  }

  if(widg)
    widg->setCanEditProperties(m_canEdit);
}

Device::AddressSettings AddressEditDialog::getSettings() const
{
  Device::AddressSettings settings;

  if(m_addressWidget && m_addressWidget->widget())
  {
    settings = m_addressWidget->widget()->getSettings();
  }
  else
  {
    // Int by default
    settings.value = 0;
  }

  settings.name = m_nameEdit->text();

  return settings;
}

Device::AddressSettings AddressEditDialog::makeDefaultSettings()
{
  Device::AddressSettings s;
  s.ioType = ossia::access_mode::BI;
  s.clipMode = ossia::bounding_mode::FREE;

  return s;
}

void AddressEditDialog::setCanRename(bool b)
{
  m_nameEdit->setEnabled(b);
}

void AddressEditDialog::setCanEditProperties(bool b)
{
  m_canEdit = b;
  m_valueTypeCBox->setEnabled(b);
  if(auto w = m_addressWidget->widget())
    w->setCanEditProperties(b);
}

void AddressEditDialog::setNodeSettings()
{
  const QString name = m_originalSettings.name;
  m_nameEdit->setText(name);
}

void AddressEditDialog::setValueSettings()
{
  const int index
      = m_valueTypeCBox->findText(State::convert::prettyType(m_originalSettings.value));
  SCORE_ASSERT(index != -1);
  SCORE_ASSERT(index < m_valueTypeCBox->count());
  if(m_valueTypeCBox->currentIndex() == index)
  {
    m_valueTypeCBox->currentIndexChanged(index);
  }
  else
  {
    m_valueTypeCBox->setCurrentIndex(
        index); // will emit currentIndexChanged(int) & call slot
  }
}
}
