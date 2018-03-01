// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddressSettingsWidget.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/IOType.hpp>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <State/Widgets/UnitWidget.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <ossia/network/base/node_attributes.hpp>
namespace Explorer
{
AddressSettingsWidget::AddressSettingsWidget(QWidget* parent)
  : QWidget(parent), m_layout{new QFormLayout}, m_none_type{false}
{
  m_ioTypeCBox = new AccessModeComboBox{this};
  m_clipModeCBox = new BoundingModeComboBox{this};
  m_repetition = new QCheckBox;
  m_repetition->setToolTip(
        tr("When repetitions are filtered, if two identical values are sent one "
           "after the other, the second is ignored."));
  m_tagsEdit = new QComboBox{this};
  m_tagsEdit->setToolTip(tr("Tags for this parameter"));
  m_tagsEdit->setEditable(true);
  m_tagsEdit->setInsertPolicy(QComboBox::InsertAtCurrent);
  m_addTagButton = new QPushButton;
  m_addTagButton->setText("+");
  connect(m_addTagButton, &QPushButton::clicked, this, [&]() {
    bool ok = false;
    auto res = QInputDialog::getText(
          this, tr("Add tag"), tr("Add a tag"), QLineEdit::Normal, QString{},
          &ok);
    if (ok)
    {
      m_tagsEdit->addItem(res);
    }
  });

  QHBoxLayout* tagLayout = new QHBoxLayout;
  tagLayout->addWidget(m_tagsEdit);
  tagLayout->addWidget(m_addTagButton);

  m_unit = new State::UnitWidget({}, this);
  m_unit->setToolTip("Set the dataspace and unit of the parameter.");

  m_description = new QLineEdit{this};

  m_layout->addRow(makeLabel(tr("Clip mode"), this), m_clipModeCBox);
  m_layout->addRow(makeLabel(tr("I/O type"), this), m_ioTypeCBox);
  m_layout->addRow(makeLabel(tr("Repetition filter"), this), m_repetition);
  m_layout->addRow(makeLabel(tr("Tags"), this), tagLayout);
  m_layout->addRow(makeLabel(tr("Unit"), this), m_unit);
  m_layout->addRow(makeLabel(tr("Description"), this), m_description);

  setLayout(m_layout);
}

AddressSettingsWidget::AddressSettingsWidget(
    AddressSettingsWidget::no_widgets_t, QWidget* parent)
  : QWidget(parent), m_layout{new QFormLayout}, m_none_type{true}
{
  m_tagsEdit = new QComboBox{this};
  m_tagsEdit->setEditable(true);
  m_tagsEdit->setInsertPolicy(QComboBox::InsertAtCurrent);
  auto addTagButton = new QPushButton;
  addTagButton->setText("+");
  connect(addTagButton, &QPushButton::clicked, this, [&]() {
    bool ok = false;
    auto res = QInputDialog::getText(
          this, tr("Add tag"), tr("Add a tag"), QLineEdit::Normal, QString{},
          &ok);
    if (ok)
    {
      m_tagsEdit->addItem(res);
    }
  });

  QHBoxLayout* tagLayout = new QHBoxLayout;
  tagLayout->addWidget(m_tagsEdit);
  tagLayout->addWidget(addTagButton);

  m_layout->addRow(tr("Tags"), tagLayout);

  setLayout(m_layout);
}

void AddressSettingsWidget::setCanEditProperties(bool b)
{
  if(m_ioTypeCBox)
    m_ioTypeCBox->setEnabled(b);
  if(m_clipModeCBox)
    m_clipModeCBox->setEnabled(b);
  if(m_repetition)
    m_repetition->setEnabled(b);
  if(m_tagsEdit)
    m_tagsEdit->setEnabled(b);
  if(m_addTagButton)
    m_addTagButton->setEnabled(b);
  if(m_description)
    m_description->setEnabled(b);
  if(m_unit)
    m_unit->setEnabled(b);
}

AddressSettingsWidget::~AddressSettingsWidget() = default;

Device::AddressSettings AddressSettingsWidget::getCommonSettings() const
{
  Device::AddressSettings settings;
  if (!m_none_type)
  {
    settings.ioType = static_cast<ossia::access_mode>(
          m_ioTypeCBox->currentData().value<int>());
    settings.clipMode = static_cast<ossia::bounding_mode>(
          m_clipModeCBox->currentData().value<int>());
    settings.repetitionFilter = static_cast<ossia::repetition_filter>(m_repetition->isChecked());
    settings.unit = m_unit->unit();
    auto txt = m_description->text();
    if(!txt.isEmpty())
    {
      ossia::net::set_description(
            settings.extendedAttributes,
            txt.toStdString());
    }
  }

  const auto tags_n = m_tagsEdit->count();
  if(tags_n > 0)
  {
    ossia::net::tags tags;
    for (int i = 0; i < tags_n; i++)
      tags.push_back(m_tagsEdit->itemText(i).toStdString());
    ossia::net::set_tags(settings.extendedAttributes, std::move(tags));
  }
  return settings;
}

void AddressSettingsWidget::setCommonSettings(
    const Device::AddressSettings& settings)
{
  if (!m_none_type)
  {
    const int ioTypeIndex = m_ioTypeCBox->findData((int)*settings.ioType);
    SCORE_ASSERT(ioTypeIndex != -1);
    m_ioTypeCBox->setCurrentIndex(ioTypeIndex);

    const int clipModeIndex = m_clipModeCBox->findData((int)settings.clipMode);
    SCORE_ASSERT(clipModeIndex != -1);
    m_clipModeCBox->setCurrentIndex(clipModeIndex);

    m_repetition->setChecked((bool)settings.repetitionFilter);

    m_unit->setUnit(settings.unit);

    // Note : for extended attributes, we should instead have some kind of checkbox ?
    if(auto desc = ossia::net::get_description(settings.extendedAttributes))
      m_description->setText(QString::fromStdString(*desc));
    else
      m_description->setText(QString{});
  }

  if(const auto& tags = ossia::net::get_tags(settings.extendedAttributes))
  {
    QStringList t;
    for(const auto& tag : *tags)
      t.push_back(QString::fromStdString(tag));
    m_tagsEdit->addItems(std::move(t));
  }
  else
  {
    m_tagsEdit->clear();
  }
}

AccessModeComboBox::AccessModeComboBox(QWidget* parent) : QComboBox{parent}
{
  setToolTip(tr("Set in which direction the communication should happen."));
  const auto& io_map = Device::AccessModeText();
  for (auto it = io_map.cbegin(); it != io_map.cend(); ++it)
  {
    addItem(it.value(), (int)it.key());
  }

  connect(
        this, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [=](int i) {
    changed(
          (ossia::access_mode) this->itemData(i).toInt());
  });
}


AccessModeComboBox::~AccessModeComboBox()
{
}


ossia::access_mode AccessModeComboBox::get() const
{
  return (ossia::access_mode) this->currentData().toInt();
}


void AccessModeComboBox::set(ossia::access_mode t)
{
  const auto& clip_map = Device::AccessModeText();
  for (int i = 0; i < clip_map.size(); i++)
  {
    if(itemData(i).toInt() == (int)t)
    {
      setCurrentIndex(i);
      break;
    }
  }
}

BoundingModeComboBox::BoundingModeComboBox(QWidget* parent) : QComboBox{parent}
{
  setToolTip(tr("Set how the values should be clipped."));
  const auto& clip_map = Device::ClipModePrettyStringMap();
  for (auto it = clip_map.cbegin(); it != clip_map.cend(); ++it)
  {
    addItem(it.value(), (int)it.key());
  }
  connect(
        this, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        [=](int i) {
    changed(
          (ossia::bounding_mode) this->itemData(i).toInt());
  });
}

BoundingModeComboBox::~BoundingModeComboBox()
{
}

ossia::bounding_mode BoundingModeComboBox::get() const
{
  return (ossia::bounding_mode) this->currentData().toInt();
}

void BoundingModeComboBox::set(ossia::bounding_mode t)
{
  const auto& clip_map = Device::ClipModePrettyStringMap();
  for (int i = 0; i < clip_map.size(); i++)
  {
    if(itemData(i).toInt() == (int)t)
    {
      setCurrentIndex(i);
      break;
    }
  }
}

}
