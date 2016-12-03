#include "ScenarioSettingsView.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <iscore/widgets/SignalUtils.hpp>

namespace Scenario
{
namespace Settings
{

View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;
  m_widg->setLayout(lay);

  // SKIN
  m_skin = new QComboBox;
  m_skin->addItems({"Default", "IEEE"});
  lay->addRow(tr("Skin"), m_skin);

  connect(m_skin, &QComboBox::currentTextChanged, this, &View::skinChanged);

  // ZOOM
  m_zoomSpinBox = new QSpinBox;
  m_zoomSpinBox->setMinimum(50);
  m_zoomSpinBox->setMaximum(300);

  connect(
      m_zoomSpinBox, SignalUtils::QSpinBox_valueChanged_int(), this,
      &View::zoomChanged);

  m_zoomSpinBox->setSuffix(tr("%"));

  lay->addRow(tr("Graphical Zoom \n (50% -- 300%)"), m_zoomSpinBox);

  // SLOT HEIGHT
  m_slotHeightBox = new QSpinBox;
  m_slotHeightBox->setMinimum(0);
  m_slotHeightBox->setMaximum(10000);

  connect(
      m_slotHeightBox,
      static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
      &View::slotHeightChanged);

  lay->addRow(tr("Default Slot Height"), m_slotHeightBox);

  // Default duration
  m_defaultDur = new iscore::TimeSpinBox;
  connect(
      m_defaultDur, &iscore::TimeSpinBox::timeChanged, this,
      [=](const QTime& t) { emit defaultDurationChanged(TimeValue{t}); });
  lay->addRow(tr("New score duration"), m_defaultDur);

  // Snapshot
  m_snapshot = new QCheckBox{m_widg};
  connect(m_snapshot, &QCheckBox::toggled, this, &View::snapshotChanged);
  lay->addRow(tr("Snapshot"), m_snapshot);

  m_sequence = new QCheckBox{m_widg};
  connect(m_sequence, &QCheckBox::toggled, this, &View::sequenceChanged);
  lay->addRow(tr("Auto-Sequence"), m_sequence);
}

void View::setSkin(const QString& val)
{
  if (val != m_skin->currentText())
  {
    int index = m_skin->findText(val);
    if (index != -1)
    {
      m_skin->setCurrentIndex(index);
    }
  }
}

void View::setZoom(const int val)
{
  if (val != m_zoomSpinBox->value())
    m_zoomSpinBox->setValue(val);
}

void View::setDefaultDuration(const TimeValue& t)
{
  auto qtime = t.toQTime();
  if (qtime != m_defaultDur->time())
    m_defaultDur->setTime(qtime);
}

void View::setSlotHeight(const double val)
{
  if (val != m_slotHeightBox->value())
    m_slotHeightBox->setValue(val);
}

void View::setSnapshot(const bool val)
{
  if (val != m_snapshot->checkState())
    m_snapshot->setChecked(val);
}

void View::setSequence(const bool val)
{
  if (val != m_sequence->checkState())
    m_sequence->setChecked(val);
}

QWidget* View::getWidget()
{
  return m_widg;
}
}
}
