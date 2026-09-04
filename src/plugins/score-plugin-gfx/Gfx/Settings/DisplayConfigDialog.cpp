#include <Gfx/Settings/DisplayConfigDialog.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Gfx::Settings
{
namespace
{
enum Column
{
  Name = 0,
  Status,
  Mode,
  Format,
  Primary,
  PosX,
  PosY,
  ColumnCount
};

//! The vocabulary the platform itself parses, so it is offered rather than
//! invented. An empty choice writes nothing and leaves the driver alone.
const QStringList& modeChoices()
{
  static const QStringList l{
      QStringLiteral(""), QStringLiteral("preferred"), QStringLiteral("current"),
      QStringLiteral("off"), QStringLiteral("skip")};
  return l;
}

const QStringList& formatChoices()
{
  static const QStringList l{
      QStringLiteral(""),          QStringLiteral("xrgb8888"),
      QStringLiteral("argb8888"),  QStringLiteral("xbgr8888"),
      QStringLiteral("abgr8888"),  QStringLiteral("rgb565"),
      QStringLiteral("bgr565"),    QStringLiteral("xrgb2101010"),
      QStringLiteral("argb2101010")};
  return l;
}
}

DisplayConfigWidget::DisplayConfigWidget(QWidget* parent)
    : QWidget{parent}
{

  auto* lay = new QVBoxLayout{this};

  const auto caps = score::gfx::displayCapabilities(QGuiApplication::platformName());
  auto* note = new QLabel{this};
  note->setWordWrap(true);
  if(caps.perOutputConfiguration)
    note->setText(
        tr("These settings take effect when score restarts: the platform reads "
           "them once, at startup."));
  else if(caps.indexedDisplaySelection)
    note->setText(tr(
        "This platform selects a display by index and cannot be told about "
        "connectors, layout or cloning. Only the Vulkan section below applies. "
        "Changes take effect when score restarts."));
  else if(caps.appliesToSystemDisplays)
    note->setText(
        tr("This system owns its displays and can rearrange them while running, "
           "so score does not do it behind your back: nothing here is applied "
           "here yet. The settings are saved, and take effect on a machine that "
           "boots without a window manager — which is what they are for."));
  else
    note->setText(
        tr("This machine has a window manager, which owns the displays: nothing "
           "here applies to it. The settings are saved for a machine that boots "
           "without one — configure them here, and they take effect there."));
  lay->addWidget(note);

  m_outputs = score::gfx::enumerateOutputs();

  m_table = new QTableWidget{(int)m_outputs.size(), ColumnCount, this};
  m_table->setHorizontalHeaderLabels(
      {tr("Output"), tr("Status"), tr("Mode"), tr("Format"), tr("Primary"), tr("X"),
       tr("Y")});
  m_table->horizontalHeader()->setSectionResizeMode(Name, QHeaderView::Stretch);
  m_table->verticalHeader()->setVisible(false);

  for(int i = 0; i < m_outputs.size(); i++)
  {
    const auto& o = m_outputs[i];

    auto* name = new QTableWidgetItem{o.name};
    name->setFlags(name->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(i, Name, name);

    auto* st = new QTableWidgetItem{o.connected ? tr("connected") : tr("disconnected")};
    st->setFlags(st->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(i, Status, st);

    auto* mode = new QComboBox;
    mode->setEditable(true);
    mode->addItems(modeChoices());
    // The modes this connector actually reports, after the keywords.
    for(const auto& m : o.modes)
      mode->addItem(m);
    m_table->setCellWidget(i, Mode, mode);

    auto* fmt = new QComboBox;
    fmt->addItems(formatChoices());
    m_table->setCellWidget(i, Format, fmt);

    auto* prim = new QCheckBox;
    m_table->setCellWidget(i, Primary, prim);

    for(int c : {PosX, PosY})
    {
      auto* sp = new QSpinBox;
      sp->setRange(-32768, 32768);
      sp->setSpecialValueText(tr("auto"));
      sp->setMinimum(-32768);
      sp->setValue(-32768); // the special value: unset
      m_table->setCellWidget(i, c, sp);
    }
  }
  lay->addWidget(m_table, 1);

  auto* globals = new QGroupBox{tr("This machine"), this};
  auto* gl = new QFormLayout{globals};
  m_editorUi = new QCheckBox;
  m_editorUi->setChecked(true);
  m_editorUi->setToolTip(
      tr("Off makes this an appliance: the render output takes the screen and "
         "there is no editor. Ctrl+Alt+Shift+E brings the editor back."));
  gl->addRow(tr("Show the editor"), m_editorUi);
  m_hwCursor = new QCheckBox;
  m_hwCursor->setChecked(true);
  gl->addRow(tr("Hardware cursor"), m_hwCursor);
  m_hideCursor = new QCheckBox;
  gl->addRow(tr("Hide the cursor"), m_hideCursor);
  m_vertical = new QCheckBox;
  gl->addRow(tr("Stack screens vertically"), m_vertical);
  m_rotation = new QComboBox;
  m_rotation->addItems({"0", "90", "180", "270"});
  gl->addRow(tr("Rotation"), m_rotation);
  m_headless = new QLineEdit;
  m_headless->setPlaceholderText(tr("e.g. 1920x1080 — render with no output at all"));
  gl->addRow(tr("Headless"), m_headless);
  m_device = new QLineEdit;
  m_device->setPlaceholderText(tr("e.g. /dev/dri/card0 — leave empty to autodetect"));
  gl->addRow(tr("DRM device"), m_device);
  lay->addWidget(globals);

  auto* vk = new QGroupBox{tr("Vulkan display (vkkhrdisplay)"), this};
  auto* vl = new QFormLayout{vk};
  auto mkIndex = [](QSpinBox*& sp, QFormLayout* l, const QString& label) {
    sp = new QSpinBox;
    sp->setRange(-1, 64);
    sp->setValue(-1);
    sp->setSpecialValueText(tr("leave to the platform"));
    l->addRow(label, sp);
  };
  mkIndex(m_vkDevice, vl, tr("Physical device index"));
  mkIndex(m_vkDisplay, vl, tr("Display index"));
  mkIndex(m_vkMode, vl, tr("Mode index"));
  lay->addWidget(vk);

  auto* save = new QPushButton{tr("Save")};
  connect(save, &QPushButton::clicked, this, &DisplayConfigWidget::save);
  auto* revert = new QPushButton{tr("Revert")};
  connect(revert, &QPushButton::clicked, this, &DisplayConfigWidget::load);
  auto* buttons = new QHBoxLayout;
  buttons->addStretch(1);
  buttons->addWidget(revert);
  buttons->addWidget(save);
  lay->addLayout(buttons);

  load();
}

void DisplayConfigWidget::load()
{
  const auto s = score::gfx::loadDisplaySettings(score::gfx::displayConfigPath());

  m_editorUi->setChecked(s.editorUi);
  m_hwCursor->setChecked(s.hardwareCursor);
  m_hideCursor->setChecked(s.hideCursor);
  m_vertical->setChecked(s.verticalLayout);
  m_rotation->setCurrentText(QString::number(s.rotation));
  m_headless->setText(s.headless);
  m_device->setText(s.device);
  m_vkDevice->setValue(s.vulkanPhysicalDeviceIndex);
  m_vkDisplay->setValue(s.vulkanDisplayIndex);
  m_vkMode->setValue(s.vulkanModeIndex);

  // Matched by connector name: a saved configuration may name an output this
  // machine does not have, which is the normal case when an appliance is
  // configured from a laptop. Those rows simply are not shown -- and are
  // dropped on save, which is why saving from the wrong machine is destructive
  // and the dialog is not a place to do it casually.
  for(const auto& o : s.outputs)
  {
    for(int i = 0; i < m_outputs.size(); i++)
    {
      if(m_outputs[i].name != o.name)
        continue;

      if(auto* c = qobject_cast<QComboBox*>(m_table->cellWidget(i, Mode)))
        c->setCurrentText(o.mode);
      if(auto* c = qobject_cast<QComboBox*>(m_table->cellWidget(i, Format)))
        c->setCurrentText(o.format);
      if(auto* c = qobject_cast<QCheckBox*>(m_table->cellWidget(i, Primary)))
        c->setChecked(o.primary);
      if(o.hasPosition)
      {
        if(auto* c = qobject_cast<QSpinBox*>(m_table->cellWidget(i, PosX)))
          c->setValue(o.x);
        if(auto* c = qobject_cast<QSpinBox*>(m_table->cellWidget(i, PosY)))
          c->setValue(o.y);
      }
      break;
    }
  }
}

score::gfx::DisplaySettings DisplayConfigWidget::collect() const
{
  score::gfx::DisplaySettings s;
  s.editorUi = m_editorUi->isChecked();
  s.hardwareCursor = m_hwCursor->isChecked();
  s.hideCursor = m_hideCursor->isChecked();
  s.verticalLayout = m_vertical->isChecked();
  s.rotation = m_rotation->currentText().toInt();
  s.headless = m_headless->text().trimmed();
  s.device = m_device->text().trimmed();
  s.vulkanPhysicalDeviceIndex = m_vkDevice->value();
  s.vulkanDisplayIndex = m_vkDisplay->value();
  s.vulkanModeIndex = m_vkMode->value();

  for(int i = 0; i < m_outputs.size(); i++)
  {
    score::gfx::DisplayOutputSettings o;
    o.name = m_outputs[i].name;

    if(auto* c = qobject_cast<QComboBox*>(m_table->cellWidget(i, Mode)))
      o.mode = c->currentText().trimmed();
    if(auto* c = qobject_cast<QComboBox*>(m_table->cellWidget(i, Format)))
      o.format = c->currentText().trimmed();
    if(auto* c = qobject_cast<QCheckBox*>(m_table->cellWidget(i, Primary)))
      o.primary = c->isChecked();

    auto* x = qobject_cast<QSpinBox*>(m_table->cellWidget(i, PosX));
    auto* y = qobject_cast<QSpinBox*>(m_table->cellWidget(i, PosY));
    if(x && y && x->value() != x->minimum() && y->value() != y->minimum())
    {
      o.x = x->value();
      o.y = y->value();
      o.hasPosition = true;
    }

    // An output nobody said anything about is left out entirely, so the file
    // stays a record of decisions rather than of defaults.
    if(!o.mode.isEmpty() || !o.format.isEmpty() || o.primary || o.hasPosition)
      s.outputs.push_back(std::move(o));
  }

  return s;
}

void DisplayConfigWidget::save()
{
  score::gfx::saveDisplaySettings(collect(), score::gfx::displayConfigPath());
}
}
