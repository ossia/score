// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusProtocolSettingsWidget.hpp"

#include "BitfocusContext.hpp"
#include "BitfocusProtocolFactory.hpp"
#include "BitfocusSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/widgets/MarginLess.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollArea>
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

  m_rootLayout = new score::MarginLess<QFormLayout>{this};
  m_rootLayout->addRow(tr("Name"), m_deviceNameEdit);
  m_rootLayout->addRow(new QLabel{
      tr("To add support for Bitfocus Companion modules:\n - Go to Settings > "
         "Package Manager\n - Install the \"Bitfocus Companion Modules\" package.")});
  m_hasInitLabel = true;
  m_rootLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  m_scroll = new QScrollArea{this};
  m_scroll->setFrameShape(QFrame::NoFrame);
  QSizePolicy sz;
  sz.setHorizontalPolicy(QSizePolicy::Ignored);
  sz.setVerticalPolicy(QSizePolicy::MinimumExpanding);
  m_scroll->setSizePolicy(sz);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_scroll->setWidgetResizable(true);
  m_rootLayout->addRow(m_scroll);
}

Device::DeviceSettings BitfocusProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = BitfocusProtocolFactory::static_concreteKey();

  BitfocusSpecificSettings osc = m_settings;
  for(auto& widg : m_widgets)
  {
    if(widg.second.getValue)
    {
      osc.configuration.emplace_back(widg.first, widg.second.getValue());
    }
  }
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

static void makeValidator(QLineEdit* widg, QString rx)
{
  auto val = new QRegularExpressionValidator;
  if(rx.startsWith("/^"))
  {
    rx.removeAt(0);
    rx.removeAt(0);
  }
  if(rx.endsWith("$/"))
  {
    rx.resize(rx.size() - 2);
  }
  val->setRegularExpression(QRegularExpression(rx));
  val->setParent(widg);
  widg->setValidator(val);

  QObject::connect(widg, &QLineEdit::textChanged, [widg](const QString& str) {
    if(!widg->validator())
      return;

    QString s = str;
    int i = 0;
    QPalette palette{widg->palette()};
    if(widg->validator()->validate(s, i) == QValidator::State::Acceptable)
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
    widg->setPalette(palette);
  });
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
        m_widgets[field.id]
            = widget{.label = lab, .widg = static_text, .getValue = {}, .setValue = {}};
      }
    }
    else if(field.type == "textinput" || field.type == "bonjourdevice")
    {
      auto widg = new QLineEdit;
      if(!field.regex.isEmpty())
        makeValidator(widg, field.regex);
      widg->setText(field.default_value.toString());
      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widg = widg, .getValue = [widg]() -> ossia::value {
        return widg->text().toStdString();
      }, .setValue = [widg](ossia::value v) {
        widg->setText(QString::fromStdString(ossia::convert<std::string>(v)));
      }};
    }
    else if(field.type == "number")
    {
      bool ok = false;
      auto tmin = field.min.typeId();
      auto tmax = field.max.typeId();
      if((tmin == QMetaType::LongLong && tmax == QMetaType::LongLong))
      {
        auto widg = new QSpinBox;
        if(tmin == QMetaType::LongLong && tmax == QMetaType::LongLong)
          widg->setRange(field.min.toInt(), field.max.toInt());
        else
          widg->setRange(0, 100000);
        widg->setValue(field.default_value.toInt());
        m_subForm->addWidget(widg);
        m_widgets[field.id]
            = {.label = lab, .widg = widg, .getValue = [widg]() -> ossia::value {
          return widg->value();
        }, .setValue = [widg](ossia::value v) {
          widg->setValue(ossia::convert<float>(v));
        }};
        ok = true;
      }
      else if(tmin == QMetaType::Double && tmax == QMetaType::Double)
      {
        auto widg = new QDoubleSpinBox;
        widg->setRange(field.min.toDouble(), field.max.toDouble());
        widg->setValue(field.default_value.toDouble());
        m_subForm->addWidget(widg);
        m_widgets[field.id]
            = {.label = lab, .widg = widg, .getValue = [widg]() -> ossia::value {
          return widg->value();
        }, .setValue = [widg](ossia::value v) {
          widg->setValue(ossia::convert<float>(v));
        }};
        ok = true;
      }
      else if(!field.regex.isEmpty())
      {
        auto widg = new QLineEdit;
        makeValidator(widg, field.regex);
        widg->setText(field.default_value.toString());
        m_subForm->addWidget(widg);
        m_widgets[field.id]
            = {.label = lab, .widg = widg, .getValue = [widg]() -> ossia::value {
          return widg->text().toInt();
        }, .setValue = [widg](ossia::value v) {
          widg->setText(QString::number(ossia::convert<int>(v)));
        }};
      }
    }
    else if(field.type == "checkbox")
    {
      auto widg = new QCheckBox;
      widg->setChecked(field.default_value.toBool() == true);
      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widg = widg, .getValue = [widg]() -> ossia::value {
        return widg->isChecked();
      }, .setValue = [widg](ossia::value v) {
        widg->setChecked(ossia::convert<bool>(v));
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
          = {.label = lab, .widg = widg, .getValue = [widg]() -> ossia::value {
        return widg->currentData().toString().toStdString();
      }, .setValue = [widg](ossia::value v) {
        auto id = QString::fromStdString(ossia::convert<std::string>(v));
        int idx = -1;
        for(int i = 0; i < widg->count(); i++)
        {
          if(widg->itemData(i) == id)
          {
            idx = i;
            break;
          }
        }
        if(idx != -1)
          widg->setCurrentIndex(idx);
      }};
    }
  }
  m_subForm->addStretch(1);
}

void BitfocusProtocolSettingsWidget::resizeEvent(QResizeEvent* res)
{
  m_scroll->setMinimumWidth(0.8 * res->size().width());
  m_scroll->setMinimumHeight(0.8 * res->size().height());
}

void BitfocusProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  if(m_hasInitLabel)
  {
    if(settings.deviceSpecificSettings.canConvert<BitfocusSpecificSettings>())
    {
      auto stgs = settings.deviceSpecificSettings.value<BitfocusSpecificSettings>();
      if(!stgs.path.isEmpty())
      {
        m_rootLayout->removeRow(1);
        m_hasInitLabel = false;
      }
    }
  }

  m_widgets.clear();
  delete m_subWidget;
  m_subWidget = new QWidget{this};
  m_subForm = new score::MarginLess<QVBoxLayout>{m_subWidget};
  m_scroll->setWidget(m_subWidget);

  m_deviceNameEdit->setText(settings.name);

  bool mustLoad = false;
  if(settings.deviceSpecificSettings.canConvert<BitfocusSpecificSettings>())
  {
    auto stgs = settings.deviceSpecificSettings.value<BitfocusSpecificSettings>();
    if(!stgs.path.isEmpty() && QDir{stgs.path}.exists())
    {
      m_deviceNameEdit->setText(stgs.name);
      if(!stgs.handler)
      {
        // First load
        auto conf = bitfocus::module_configuration{};
        {
          if(!stgs.product.isEmpty())
          {
            conf["product"] = stgs.product;
          }
        }
        stgs.handler = std::make_shared<bitfocus::module_handler>(
            stgs.path, stgs.entrypoint, stgs.nodeVersion, stgs.apiVersion,
            std::move(conf));
        connect(
            stgs.handler.get(), &bitfocus::module_handler::configurationParsed, this,
            [this] { updateFields(); });
      }
      else
      {
        // Device already loaded, we're editing
        mustLoad = true;
      }
    }

    m_settings = stgs;

    if(mustLoad)
    {
      // 1. Load the fields
      updateFields();

      // 2. Load our saved values
      for(auto& [k, v] : stgs.configuration)
      {
        if(auto member = this->m_widgets.find(k); member != m_widgets.end())
        {
          if(member->second.setValue)
            member->second.setValue(v);
        }
      }
    }
  }
}
}
