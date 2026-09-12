// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioSettingsView.hpp"

#include <Scenario/Settings/SkinEditorWidget.hpp>

#include <Process/UIPlacement.hpp>

#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Skin.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/FilePath.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/TimeSpinBox.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QtColorWidgets/ColorWheel>

#include <wobjectimpl.h>

namespace Scenario
{
namespace Settings
{


View::View()
{
  m_widg = new score::FormWidget{tr("User interface")};

  auto lay = m_widg->layout();
  lay->setLabelAlignment(Qt::AlignLeft);
  lay->setSpacing(10);

  {
    auto subw = new QWidget;
    auto sublay = new score::MarginLess<QHBoxLayout>{subw};
    m_editor = new QLineEdit{};
    auto btn = new QPushButton{tr("Browse..."), m_widg};
    connect(btn, &QPushButton::pressed, this, [this, subw] {
      auto file = QFileDialog::getOpenFileName(
          subw, tr("Default editor"), score::pickerStartFolder(m_editor->text()));
      if(!file.isEmpty())
      {
        m_editor->setText(file);
        DefaultEditorChanged(file);
      }
    });

    connect(m_editor, &QLineEdit::editingFinished, this, [this] {
      DefaultEditorChanged(m_editor->text());
    });
    sublay->addWidget(m_editor);
    sublay->addWidget(btn);

    lay->addRow(tr("Default editor"), subw);
  }

  // Process UIs
  SETTINGS_UI_COMBOBOX_SETUP(
      "Script editors", ScriptEditorPlacement, Process::UIPlacementSettings::names());
  score::setHelp(
      m_ScriptEditorPlacement,
      tr("Where the code editors of processes (JS, shaders, DSP...) open: "
         "in a separate window, as a tab of the right pane next to the inspector, "
         "or in the central area in place of the score."));
  SETTINGS_UI_COMBOBOX_SETUP(
      "Behind central editors", ScriptEditorPreview,
      Process::UIPlacementSettings::previewNames());
  score::setHelp(
      m_ScriptEditorPreview,
      tr("How a script editor opened in the central view looks: drawn without "
         "chrome nor background over what the document shows behind itself (a "
         "Background device, a watched texture port) when there is one, for live "
         "coding; or a plain editor."));
  SETTINGS_UI_COMBOBOX_SETUP(
      "Process UIs", ProcessUIPlacement, Process::UIPlacementSettings::names());
  score::setHelp(
      m_ProcessUIPlacement,
      tr("Where the custom user interfaces of processes open, when they can be "
         "embedded (JS UIs). Plug-ins with a native window (VST, LV2, CLAP...) "
         "always open in a separate window."));
  // ZOOM. Below Qt 6.6 the scale factor cannot be changed on a running
  // application, so the label says so rather than silently doing nothing.
  m_zoomSpinBox = new QSpinBox;
  m_zoomSpinBox->setMinimum(100);
  m_zoomSpinBox->setMaximum(200);
  m_zoomSpinBox->setSuffix(tr("%"));
  connect(
      m_zoomSpinBox, SignalUtils::QSpinBox_valueChanged_int(), this, &View::zoomChanged);
  lay->addRow(
      score::canSetGlobalScaleFactorLive() ? tr("Graphical Zoom")
                                           : tr("Graphical Zoom (needs restart)"),
      m_zoomSpinBox);

  // SLOT HEIGHT
  m_slotHeightBox = new QSpinBox;
  m_slotHeightBox->setMinimum(0);
  m_slotHeightBox->setMaximum(10000);

  connect(
      m_slotHeightBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
      this, &View::SlotHeightChanged);

  lay->addRow(tr("Default Slot Height"), m_slotHeightBox);

  // Default duration
  m_defaultDur = new score::TimeSpinBox;
  connect(
      m_defaultDur, &score::TimeSpinBox::timeChanged, this,
      [this](const TimeVal& t) { DefaultDurationChanged(t); });
  lay->addRow(tr("New score duration"), m_defaultDur);

  m_sequence = new QCheckBox{tr("Auto-Sequence"), m_widg};
  connect(m_sequence, &QCheckBox::toggled, this, &View::AutoSequenceChanged);
  lay->addRow(m_sequence);

  SETTINGS_UI_SPINBOX_SETUP("Update Rate (ms)", UpdateRate);
  score::setHelp(
      this->m_UpdateRate,
      tr("Rate at which various events are processed in the software: "
         "UI updates from the execution engine, audio plug-in UIs..."));
  m_UpdateRate->setRange(1, 50);
  SETTINGS_UI_SPINBOX_SETUP("Execution Refresh Rate (hz)", ExecutionRefreshRate);

  SETTINGS_UI_TOGGLE_SETUP("Execution GUI update", ExecutionUpdate);

  score::setHelp(
      this->m_ExecutionRefreshRate,
      tr("Refresh rate of the main view when the score executes, in hertz. "
         "Set a lower value to leave more CPU for the actual processing."));
  m_ExecutionRefreshRate->setRange(20, 500);
  SETTINGS_UI_TOGGLE_SETUP("Time Bar", TimeBar);
  SETTINGS_UI_TOGGLE_SETUP("Show musical metrics", MeasureBars);
  SETTINGS_UI_TOGGLE_SETUP("Magnetism on musical metrics", MagneticMeasures);
}

SETTINGS_UI_COMBOBOX_IMPL(ScriptEditorPlacement)
SETTINGS_UI_COMBOBOX_IMPL(ProcessUIPlacement)
SETTINGS_UI_COMBOBOX_IMPL(ScriptEditorPreview)
SETTINGS_UI_SPINBOX_IMPL(UpdateRate)
SETTINGS_UI_SPINBOX_IMPL(ExecutionRefreshRate)
SETTINGS_UI_TOGGLE_IMPL(TimeBar)
SETTINGS_UI_TOGGLE_IMPL(MeasureBars)
SETTINGS_UI_TOGGLE_IMPL(MagneticMeasures)
SETTINGS_UI_TOGGLE_IMPL(ExecutionUpdate)

void View::setDefaultEditor(QString val)
{
  if(val != m_editor->text())
    m_editor->setText(val);
}

void View::setDefaultDuration(const TimeVal& t)
{
  m_defaultDur->setTime(t);
}

void View::setSlotHeight(const double val)
{
  if(val != m_slotHeightBox->value())
    m_slotHeightBox->setValue(val);
}

void View::setAutoSequence(const bool val)
{
  if(val != m_sequence->isChecked())
    m_sequence->setChecked(val);
}

QWidget* View::getWidget()
{
  if(!m_tabs)
  {
    m_tabs = new QTabWidget;
    m_tabs->addTab(m_widg, tr("Interface"));

    // The skin editor is inlined rather than opened as a dialog: it edits the
    // live skin, so there is nothing to confirm or cancel.
    m_skinEditor = new SkinEditorWidget;
    m_tabs->addTab(m_skinEditor, tr("Skin"));
    connect(
        m_skinEditor, &SkinEditorWidget::skinChanged, this,
        [this](const QString& s) { SkinChanged(s); });

    // Whatever the presenter told us before this existed.
    if(!m_pendingSkin.isEmpty())
      m_skinEditor->setSkin(m_pendingSkin);
  }
  return m_tabs;
}

void View::setSkin(const QString& val)
{
  m_pendingSkin = val;
  if(m_skinEditor)
    m_skinEditor->setSkin(val);
}

void View::setZoom(const int val)
{
  if(val != m_zoomSpinBox->value())
    m_zoomSpinBox->setValue(val);
}
}
}
