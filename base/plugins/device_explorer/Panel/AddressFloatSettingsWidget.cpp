#include "AddressFloatSettingsWidget.hpp"

#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

#include "NodeFactory.hpp"


AddressFloatSettingsWidget::AddressFloatSettingsWidget(QWidget *parent)
  : AddressSettingsWidget(parent)
{

  buildGUI();
}

void
AddressFloatSettingsWidget::buildGUI()
{
  QLabel *ioTypeLabel = new QLabel(tr("I/O type"), this);
  m_ioTypeCBox = new QComboBox(this);

  QLabel *valueLabel = new QLabel(tr("Value"), this);
  m_valueSBox = new QDoubleSpinBox(this);

  QLabel *minLabel = new QLabel(tr("Min"), this);
  m_minSBox = new QDoubleSpinBox(this);

  QLabel *maxLabel = new QLabel(tr("Max"), this);
  m_maxSBox = new QDoubleSpinBox(this);

  QLabel *unitLabel = new QLabel(tr("Unit"), this);
  m_unitCBox = new QComboBox(this);

  QLabel *clipModeLabel  = new QLabel(tr("Clip mode"), this);
  m_clipModeCBox = new QComboBox(this);

  QLabel *priorityLabel  = new QLabel(tr("Priority"), this);
  m_prioritySBox = new QSpinBox(this);

  QLabel *tagsLabel  = new QLabel(tr("Tags"), this);
  m_tagsEdit = new QLineEdit(this);

  QGridLayout *gLayout = new QGridLayout;
  
  gLayout->addWidget(ioTypeLabel, 0, 0, 1, 1);
  gLayout->addWidget(m_ioTypeCBox, 0, 1, 1, 1);

  gLayout->addWidget(valueLabel, 1, 0, 1, 1);
  gLayout->addWidget(m_valueSBox, 1, 1, 1, 1);

  gLayout->addWidget(unitLabel, 1, 2, 1, 1);
  gLayout->addWidget(m_unitCBox, 1, 3, 1, 1);

  gLayout->addWidget(minLabel, 2, 0, 1, 1);
  gLayout->addWidget(m_minSBox, 2, 1, 1, 1);

  gLayout->addWidget(maxLabel, 2, 2, 1, 1);
  gLayout->addWidget(m_maxSBox, 2, 3, 1, 1);

  gLayout->addWidget(clipModeLabel, 3, 0, 1, 1);
  gLayout->addWidget(m_clipModeCBox, 3, 1, 1, 1);

  gLayout->addWidget(priorityLabel, 4, 0, 1, 1);
  gLayout->addWidget(m_prioritySBox, 4, 1, 1, 1);

  gLayout->addWidget(tagsLabel, 5, 0, 1, 1);
  gLayout->addWidget(m_tagsEdit, 5, 1, 1, 1);

  populateIOTypes(m_ioTypeCBox);
  populateUnit(m_unitCBox);
  populateClipMode(m_clipModeCBox);

  setLayout(gLayout);

  setDefaults();
}

void
AddressFloatSettingsWidget::setDefaults()
{
  Q_ASSERT(m_ioTypeCBox);
  Q_ASSERT(m_valueSBox);
  
  m_ioTypeCBox->setCurrentIndex(0);

  m_valueSBox->setValue(0.0);

  m_minSBox->setMinimum(-100000.0); //?
  m_minSBox->setMaximum(100000.0); //?
  m_minSBox->setValue(0.0);

  m_maxSBox->setMinimum(-100000.0); //?
  m_maxSBox->setMaximum(100000.0); //?
  m_maxSBox->setValue(1.0);

  m_unitCBox->setCurrentIndex(0);

  m_clipModeCBox->setCurrentIndex(0);

  m_prioritySBox->setMinimum(0);
  m_prioritySBox->setMaximum(10000); //?
  m_prioritySBox->setSingleStep(1);
  m_prioritySBox->setValue(0);

  m_tagsEdit->setText("");
}

QList<QString>
AddressFloatSettingsWidget::getSettings() const
{
  Q_ASSERT(m_ioTypeCBox);

  QList<QString> list;
  list.append(m_ioTypeCBox->currentText());
  list.append(QString::number(m_valueSBox->value()));
  list.append(QString::number(m_minSBox->value()));
  list.append(QString::number(m_maxSBox->value()));
  list.append(m_unitCBox->currentText());
  list.append(m_clipModeCBox->currentText());
  list.append(QString::number(m_prioritySBox->value()));
  list.append(m_tagsEdit->text()); //TODO: TagListWidget

  return list;
}

void
AddressFloatSettingsWidget::setSettings(const QList<QString> &settings)
{
  Q_ASSERT(m_ioTypeCBox);
  
  Q_ASSERT(settings.size() == 8);

  const QString &ioTypeString = settings.at(0);
  const int ioTypeIndex = m_ioTypeCBox->findText(ioTypeString);
  if (ioTypeIndex != -1) {
    m_ioTypeCBox->setCurrentIndex(ioTypeIndex);
  }
  else {
    qDebug() << tr("Unknown I/O type: %1").arg(ioTypeString)<<"\n";
  }

  m_valueSBox->setValue(settings.at(1).toInt());
  m_minSBox->setValue(settings.at(2).toInt());
  m_maxSBox->setValue(settings.at(3).toInt());
  
  const QString &unitString = settings.at(4);
  const int unitIndex = m_unitCBox->findText(unitString);
  if (unitIndex != -1) {
    m_unitCBox->setCurrentIndex(unitIndex);
  }
  else {
    qDebug() << tr("Unknown unit type: %1").arg(unitString)<<"\n";
  }

  const QString &clipModeString = settings.at(5);
  const int clipModeIndex = m_clipModeCBox->findText(clipModeString);
  if (clipModeIndex != -1) {
    m_clipModeCBox->setCurrentIndex(clipModeIndex);
  }
  else {
    qDebug() << tr("Unknown clip mode: %1").arg(clipModeString)<<"\n";
  }

  m_prioritySBox->setValue(settings.at(6).toInt());
  
  m_tagsEdit->setText(settings.at(7));
}


