// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusProtocolSettingsWidget.hpp"

#include "BitfocusContext.hpp"
#include "BitfocusProtocolFactory.hpp"
#include "BitfocusSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/widgets/ValidationPalette.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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

  auto set = [&osc](const QString& id, ossia::value v) {
    auto it = ossia::find_if(osc.configuration, [&id](const auto& kv) {
      return kv.first == id;
    });
    if(it != osc.configuration.end())
      it->second = std::move(v);
    else
      osc.configuration.emplace_back(id, std::move(v));
  };

  const bitfocus::module_data* model = osc.handler ? &osc.handler->model() : nullptr;
  if(model && m_fieldsLoaded)
  {
    // Keys the module keeps in its configuration without showing them
    for(auto& [k, v] : model->config)
      if(k != "product" && !m_widgets.contains(k))
        if(ossia::none_of(osc.configuration, [&k](auto& kv) { return kv.first == k; }))
          osc.configuration.emplace_back(k, ossia::qt::qt_to_ossia{}(v));
    osc.upgradeIndex = model->upgradeIndex;
  }

  for(auto& [id, widg] : m_widgets)
  {
    if(!widg.getValue)
      continue;
    auto v = widg.getValue();
    if(v == widg.shown)
    {
      if(!widg.source)
        continue;
      v = *widg.source;
    }
    set(id, std::move(v));
  }
  s.deviceSpecificSettings = QVariant::fromValue(osc);

  return s;
}

static void makeValidator(QLineEdit* widg, QString rx)
{
  auto val = new QRegularExpressionValidator;
  if(rx.startsWith("/^"))
  {
    rx.remove(0, 2);
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
    score::setInputValidity(
        *widg, widg->validator()->validate(s, i) == QValidator::State::Acceptable
                   ? score::InputValidity::Valid
                   : score::InputValidity::Invalid);
  });
}

void BitfocusProtocolSettingsWidget::resetFields()
{
  m_widgets.clear();
  m_fieldsLoaded = false;
  delete m_subWidget;
  m_subWidget = new QWidget{this};
  m_subForm = new score::MarginLess<QVBoxLayout>{m_subWidget};
  m_scroll->setWidget(m_subWidget);
}

static ossia::value toSetting(const bitfocus::module_data::config_field& field, QVariant v)
{
  return ossia::qt::qt_to_ossia{}(bitfocus::toModuleValue(field, v).toVariant());
}

// A number without default stays undefined until it is set, as in companion
template <typename Spin>
static void setupEmptyNumber(Spin* widg, const bitfocus::module_data::config_field& field)
{
  if(field.hasDefault())
    return;
  widg->setMinimum(widg->minimum() - widg->singleStep());
  widg->setSpecialValueText(QStringLiteral(" "));
}

template <typename Spin>
static bool isEmptyNumber(Spin* widg, const bitfocus::module_data::config_field& field)
{
  return !field.hasDefault() && widg->value() == widg->minimum();
}

static QString colorToString(const QVariant& v)
{
  if(v.typeId() == QMetaType::QString)
    return v.toString();
  return QColor::fromRgb(QRgb(v.toLongLong() & 0xFFFFFF)).name();
}

void BitfocusProtocolSettingsWidget::updateFields()
{
  if(!m_settings.handler)
    return;

  resetFields();

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
      if(!field.tooltip.isEmpty())
        lab->setToolTip(field.tooltip);
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
    else if(field.type == "number")
    {
      const double min = field.min.isValid() ? field.min.toDouble() : -1e9;
      const double max = field.max.isValid() ? field.max.toDouble() : 1e9;
      if(field.isInteger())
      {
        auto widg = new QSpinBox;
        widg->setRange(
            std::max(min, (double)std::numeric_limits<int>::lowest()),
            std::min(max, (double)std::numeric_limits<int>::max()));
        if(field.step.isValid())
          widg->setSingleStep(std::max(1, field.step.toInt()));
        setupEmptyNumber(widg, field);
        widg->setValue(field.hasDefault() ? field.default_value.toInt() : widg->minimum());
        m_subForm->addWidget(widg);
        m_widgets[field.id]
            = {.label = lab, .widg = widg, .getValue = [widg, field]() -> ossia::value {
          if(isEmptyNumber(widg, field))
            return std::string{};
          return toSetting(field, widg->value());
        }, .setValue = [widg](ossia::value v) {
          if(v.get_type() == ossia::val_type::STRING && ossia::convert<std::string>(v).empty())
            widg->setValue(widg->minimum());
          else
            widg->setValue(ossia::convert<int>(v));
        }};
      }
      else
      {
        auto widg = new QDoubleSpinBox;
        widg->setRange(min, max);
        widg->setDecimals(4);
        if(field.step.isValid())
          widg->setSingleStep(field.step.toDouble());
        setupEmptyNumber(widg, field);
        widg->setValue(
            field.hasDefault() ? field.default_value.toDouble() : widg->minimum());
        m_subForm->addWidget(widg);
        m_widgets[field.id]
            = {.label = lab, .widg = widg, .getValue = [widg, field]() -> ossia::value {
          if(isEmptyNumber(widg, field))
            return std::string{};
          return toSetting(field, widg->value());
        }, .setValue = [widg](ossia::value v) {
          if(v.get_type() == ossia::val_type::STRING && ossia::convert<std::string>(v).empty())
            widg->setValue(widg->minimum());
          else
            widg->setValue(ossia::convert<double>(v));
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
    else if(field.type == "dropdown")
    {
      auto widg = new QComboBox;
      widg->setEditable(field.allowCustom);
      const auto default_v = field.default_value.toString();
      for(const auto& choice : field.choices)
        widg->addItem(choice.label, choice.id);
      // A default outside of the choices stays selected as-is, as in companion
      if(int idx = widg->findData(default_v); idx != -1)
        widg->setCurrentIndex(idx);
      else if(field.allowCustom)
        widg->setEditText(default_v);
      else
        widg->setCurrentIndex(-1);

      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widg = widg, .getValue = [widg, field]() -> ossia::value {
        if(widg->isEditable() && widg->currentText() != widg->currentData().toString()
           && widg->findText(widg->currentText()) == -1)
          return toSetting(field, widg->currentText());
        if(widg->currentIndex() < 0)
          return toSetting(field, field.default_value);
        return toSetting(field, widg->currentData());
      }, .setValue = [widg](ossia::value v) {
        auto id = QString::fromStdString(ossia::convert<std::string>(v));
        if(int idx = widg->findData(id); idx != -1)
          widg->setCurrentIndex(idx);
        else if(widg->isEditable())
          widg->setEditText(id);
      }};
    }
    else if(field.type == "multidropdown")
    {
      auto widg = new QListWidget;
      QStringList defaults;
      for(const auto& v : field.default_json.toArray())
        defaults.push_back(v.toVariant().toString());
      for(const auto& choice : field.choices)
      {
        auto item = new QListWidgetItem{choice.label, widg};
        item->setData(Qt::UserRole, choice.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(defaults.contains(choice.id) ? Qt::Checked : Qt::Unchecked);
      }
      widg->setMaximumHeight(150);
      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widg = widg, .getValue = [widg, field]() -> ossia::value {
        QVariantList ids;
        for(int i = 0; i < widg->count(); i++)
          if(widg->item(i)->checkState() == Qt::Checked)
            ids.push_back(widg->item(i)->data(Qt::UserRole));
        return toSetting(field, ids);
      }, .setValue = [widg](ossia::value v) {
        QStringList ids;
        for(auto& e : ossia::convert<std::vector<ossia::value>>(v))
          ids.push_back(QString::fromStdString(ossia::convert<std::string>(e)));
        for(int i = 0; i < widg->count(); i++)
          widg->item(i)->setCheckState(
              ids.contains(widg->item(i)->data(Qt::UserRole).toString()) ? Qt::Checked
                                                                         : Qt::Unchecked);
      }};
    }
    else if(field.type == "colorpicker")
    {
      auto widg = new QLineEdit;
      widg->setText(colorToString(field.default_value));
      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widg = widg, .getValue = [widg, field]() -> ossia::value {
        if(field.default_json.isString() || field.returnType == "string")
          return widg->text().toStdString();
        return (int)(QColor(widg->text()).rgb() & 0xFFFFFF);
      }, .setValue = [widg](ossia::value v) {
        widg->setText(colorToString(v.apply(ossia::qt::ossia_to_qvariant{})));
      }};
    }
    else
    {
      // textinput, secret-text, bonjour-device, custom-variable
      auto widg = new QLineEdit;
      if(field.type.startsWith("secret"))
        widg->setEchoMode(QLineEdit::PasswordEchoOnEdit);
      if(!field.regex.isEmpty())
        makeValidator(widg, field.regex);
      widg->setText(field.default_value.toString());
      m_subForm->addWidget(widg);
      m_widgets[field.id]
          = {.label = lab, .widg = widg, .getValue = [widg, field]() -> ossia::value {
        return toSetting(field, widg->text());
      }, .setValue = [widg](ossia::value v) {
        widg->setText(QString::fromStdString(ossia::convert<std::string>(v)));
      }};
    }
  }
  m_subForm->addStretch(1);
  m_fieldsLoaded = true;

  // The declared default, then what the module holds, then what the user had set
  for(auto& field : m.config_fields)
    loadValue(
        field.id, field.hasDefault() ? std::optional{ossia::qt::qt_to_ossia{}(
                                           field.default_json.toVariant())}
                                     : std::nullopt);
  for(auto& [k, v] : m.config)
    loadValue(k, ossia::qt::qt_to_ossia{}(bitfocus::widenFloat(v)));
  for(auto& [k, v] : m_settings.configuration)
    loadValue(k, v);
}

void BitfocusProtocolSettingsWidget::loadValue(
    const QString& id, std::optional<ossia::value> v)
{
  auto member = m_widgets.find(id);
  if(member == m_widgets.end() || !member->second.setValue)
    return;
  auto& w = member->second;
  if(v)
    w.setValue(*v);
  w.shown = w.getValue();
  w.source = std::move(v);
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

  if(!settings.deviceSpecificSettings.canConvert<BitfocusSpecificSettings>())
  {
    resetFields();
    m_settings = {};
    m_deviceNameEdit->setText(settings.name);
    return;
  }

  auto stgs = settings.deviceSpecificSettings.value<BitfocusSpecificSettings>();
  stgs.deduplicateConfiguration();

  // Re-picking the module we already show would restart its process.
  if(m_settings.handler && !stgs.path.isEmpty() && stgs.path == m_settings.path
     && stgs.id == m_settings.id && stgs.product == m_settings.product)
    return;

  resetFields();
  m_deviceNameEdit->setText(settings.name);

  if(stgs.path.isEmpty() || !QDir{stgs.path}.exists())
  {
    m_settings = stgs;
    return;
  }

  // The enumerator lists modules by a label that is not a usable device name.
  if(!stgs.name.isEmpty() && settings.name == stgs.enumeratorLabel())
    m_deviceNameEdit->setText(stgs.name);

  // The handler is not serialized: start the module to get its config fields.
  if(!stgs.handler)
    stgs.handler = stgs.makeHandler(m_deviceNameEdit->text());

  m_settings = stgs;

  disconnect(m_configurationParsed);
  m_configurationParsed = connect(
      m_settings.handler.get(), &bitfocus::module_handler::configurationParsed, this,
      [this, h = std::weak_ptr{m_settings.handler}] {
    if(h.lock() == m_settings.handler)
      updateFields();
  });

  disconnect(m_configurationSaved);
  m_configurationSaved = connect(
      m_settings.handler.get(), &bitfocus::module_handler::configurationSaved, this,
      [this, h = std::weak_ptr{m_settings.handler}] {
    auto handler = h.lock();
    if(!handler || handler != m_settings.handler || !m_fieldsLoaded)
      return;
    for(auto& [k, v] : handler->model().config)
      if(auto w = m_widgets.find(k); w != m_widgets.end() && w->second.getValue
         && w->second.getValue() == w->second.shown)
        loadValue(k, ossia::qt::qt_to_ossia{}(bitfocus::widenFloat(v)));
  });

  // An already-running device answered long before we connected.
  if(!m_settings.handler->model().config_fields.empty())
    updateFields();
}
}
