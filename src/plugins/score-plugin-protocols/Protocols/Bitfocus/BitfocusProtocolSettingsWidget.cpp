// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusProtocolSettingsWidget.hpp"

#include "BitfocusContext.hpp"
#include "BitfocusProtocolFactory.hpp"
#include "BitfocusSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVariant>

#include <wobjectimpl.h>

namespace Protocols
{

BitfocusProtocolSettingsWidget::BitfocusProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("OSCdevice");
  checkForChanges(m_deviceNameEdit);

  m_rootLayout = new QFormLayout{this};
  m_rootLayout->addRow(tr("Name"), m_deviceNameEdit);
}

Device::DeviceSettings BitfocusProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = BitfocusProtocolFactory::static_concreteKey();

  BitfocusSpecificSettings osc = m_settings;
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

void BitfocusProtocolSettingsWidget::updateFields()
{
  if(!m_settings.handler)
    return;

  auto& m = m_settings.handler->model();
  for(auto& field : m.config_fields)
  {
    // 1. Add the field label
    QLabel* lab{};
    if(!field.label.isEmpty())
    {
      lab = new QLabel{};
      lab->setWordWrap(true);
      lab->setTextFormat(Qt::RichText);
      lab->setText(QString("<b>%1</b>").arg(field.label));
      m_subForm->addWidget(lab);
    }

    // 2. Create the widget proper
    if(field.type == "static-text")
    {
      if(auto str = field.value.toString(); !str.isEmpty())
      {
        auto static_text = new QLabel{};
        static_text->setWordWrap(true);
        static_text->setTextFormat(Qt::RichText);
        static_text->setText(str);

        m_subForm->addWidget(static_text);
        m_widgets[field.id] = {.label = lab, .widget = static_text, .getValue = {}};
      }
    }
    else if(field.type == "textinput" || field.type == "bonjourdevice")
    {
      auto widg = new QLineEdit;
      if(!field.regex.isEmpty())
      {
        auto val = new QRegularExpressionValidator;
        val->setRegularExpression(QRegularExpression(field.regex));
        val->setParent(widg);
        widg->setValidator(val);
      }
      widg->setText(field.default_value.toString());
      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widget = widg, .getValue = [widg]() -> QVariant {
        return widg->text();
      }};
    }
    else if(field.type == "number")
    {
      auto widg = new QDoubleSpinBox;
      widg->setRange(field.min, field.max);
      widg->setValue(field.default_value.toDouble());
      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widget = widg, .getValue = [widg]() -> QVariant {
        return widg->value();
      }};
    }
    else if(field.type == "checkbox")
    {
      auto widg = new QCheckBox;
      widg->setChecked(field.default_value.toBool() == true);
      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widget = widg, .getValue = [widg]() -> QVariant {
        return widg->isChecked();
      }};
    }
    else if(field.type == "choices" || field.type == "dropdown")
    {
      auto widg = new QComboBox;
      int i = 0;
      int default_i = -1;
      auto default_v = field.default_value.toString();
      for(const auto& choice : field.choices)
      {
        widg->addItem(choice.label, choice.id);
        if(choice.id == default_v)
          default_i = i;
        i++;
      }
      if(default_i != -1)
        widg->setCurrentIndex(default_i);

      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widget = widg, .getValue = [widg]() -> QVariant {
        return widg->currentData();
      }};
    }
  }
}

void BitfocusProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  delete m_subWidget;
  m_subWidget = new QWidget{this};
  m_subForm = new QVBoxLayout{m_subWidget};
  m_rootLayout->addRow(m_subWidget);

  m_deviceNameEdit->setText(settings.name);

  if(settings.deviceSpecificSettings.canConvert<BitfocusSpecificSettings>())
  {
    auto set = settings.deviceSpecificSettings.value<BitfocusSpecificSettings>();
    if(!set.path.isEmpty() && QDir{set.path}.exists())
    {
      set.handler = std::make_shared<bitfocus::module_handler>(set.path);
      connect(
          set.handler.get(), &bitfocus::module_handler::configurationParsed, this,
          [this] { updateFields(); });
    }

    m_settings = set;
  }
}
}
