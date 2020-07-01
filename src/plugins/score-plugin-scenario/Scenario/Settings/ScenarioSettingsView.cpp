// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioSettingsView.hpp"

#include <score/model/Skin.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/FormWidget.hpp>
#include <score/widgets/MarginLess.hpp>

#include <score/application/ApplicationContext.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <Library/LibrarySettings.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QtColorWidgets/ColorWheel>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QDir>

#include <wobjectimpl.h>

namespace Scenario
{
namespace Settings
{

class ThemeDialog : public QDialog
{
    W_OBJECT(ThemeDialog)

public:
  QHBoxLayout layout;
  QFormLayout sublay;
  QListWidget list;
  QLineEdit hexa;
  QLineEdit rgb;
  QPushButton save{tr("Save")};
  color_widgets::ColorWheel wheel;

  void skinSaved(const QString& arg_1) W_SIGNAL(skinSaved, arg_1);

  ThemeDialog(const QString& skinFile, QWidget* p) : QDialog{p}
  {
    setWindowTitle(tr("Edit skin"));

    layout.addWidget(&list);
    layout.addLayout(&sublay);

    wheel.setMinimumSize(QSize{100,100});
    wheel.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sublay.setWidget(0, QFormLayout::SpanningRole, &wheel);

    sublay.addRow(tr("HEX"),&hexa);
    sublay.addRow(tr("RGB"),&rgb);
    sublay.addWidget(&save);
    this->setLayout(&layout);
    score::Skin& s = score::Skin::instance();
    for (auto& col : s.getColors())
    {
      QPixmap p{16, 16};
      p.fill(col.first);
      list.addItem(new QListWidgetItem(p, col.second));
    }

    connect(&list, &QListWidget::currentItemChanged, this, [&](QListWidgetItem* cur, auto prev) {
      if (cur)
        wheel.setColor(s.fromString(cur->text())->color());
    });

    connect(&wheel, &color_widgets::ColorWheel::colorChanged, this, [&](QColor c) {
      if (list.currentItem())
      {
        if (auto brush = s.fromString(list.currentItem()->text()))
          brush->reload(c);
        QPixmap p{16, 16};
        p.fill(c);
        list.currentItem()->setIcon(p);
        s.changed();
        hexa.setText(c.name(QColor::HexRgb));
        rgb.setText(QString("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue()));
      }
    });

    connect(&hexa, &QLineEdit::textChanged, this, [this, &s](const QString& txt) {
      auto item = list.currentItem();
      if (!item)
        return;
      if (!QColor::isValidColor(txt))
        return;
      auto c = QColor(txt);
      auto brush = s.fromString(item->text());
      if (brush->color() != c)
      {
        QPixmap p{16, 16};
        p.fill(c);
        item->setIcon(p);
        brush->reload(c);
        rgb.setText(QString("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue()));
        s.changed();
        wheel.setColor(c);
      }
    });

    connect(&save, &QPushButton::clicked, this, [&] {
      auto f = QFileDialog::getSaveFileName(
          nullptr, tr("Save edited skin file"), skinFile, tr("*.json"));
      if (f.isEmpty())
        return;
      QFile fl{f};
      fl.open(QIODevice::WriteOnly);
      if (!fl.isOpen())
        return;

      QJsonObject obj;
      for (auto& col : score::Skin::instance().getColors())
      {
       obj.insert(col.second, QJsonArray{col.first.red(), col.first.green(), col.first.blue() });
      }

      QJsonDocument doc;
      doc.setObject(obj);
      fl.write(doc.toJson());
      skinSaved(f);
    });
  }
};
W_OBJECT_IMPL(ThemeDialog);

View::View()
{
  m_widg = new score::FormWidget{tr("User interface")};

  auto lay = m_widg->layout();
  lay->setLabelAlignment(Qt::AlignLeft);
  lay->setSpacing(10);

  // SKIN
  {

    m_skin = new QComboBox;
    m_skin->addItem("Default", ":/skin/DefaultSkin.json");

    const QString skinPath = score::AppContext().settings<Library::Settings::Model>().getPath() + "/Skins/";
    QDir skinDir(skinPath, "*.json");
    auto skinList = skinDir.entryList();
    for(const auto& skin: skinList)
    {
      m_skin->addItem(skin, QVariant{skinPath+"/"+skin});
    }

    auto ls = new QPushButton{tr("Browse...")};
    ls->setMaximumWidth(100);
    connect(ls, &QPushButton::clicked, this, [=] {
      auto f = QFileDialog::getOpenFileName(nullptr, tr("Load skin"), tr("*.json"));
      if(!f.isEmpty())
      {
        SkinChanged(f);
      }
    });

    auto es = new QPushButton{tr("Edit")};
    es->setMaximumWidth(100);

    connect(es, &QPushButton::clicked, this, [&] {
      QString skinToEditPath = m_skin->currentData().toString();
      if(m_skin->currentText() == tr("Default"))
          skinToEditPath = skinPath;
      ThemeDialog d{skinToEditPath, nullptr};
      connect(&d, &ThemeDialog::skinSaved, this, [&](const QString& skin) {
        SkinChanged(skin);
      });

      d.exec();
    });

    auto subw = new QWidget;
    auto sublay = new score::MarginLess<QHBoxLayout>{subw};
    sublay->addWidget(m_skin);
    sublay->addWidget(ls);
    sublay->addWidget(es);

    lay->addRow(tr("Skin"), subw);

    connect(m_skin, SignalUtils::QComboBox_currentIndexChanged_int(), this, [=] (int index){
      auto skinPath = m_skin->itemData(index).toString();
      SkinChanged(skinPath);
    });
  }

  {
    auto subw = new QWidget;
    auto sublay = new score::MarginLess<QHBoxLayout>{subw};
    m_editor = new QLineEdit{};
    auto btn = new QPushButton{tr("Browse..."), m_widg};
    connect(btn, &QPushButton::pressed, this, [=] {
      auto file = QFileDialog::getOpenFileName(subw, tr("Default editor"));
      if (!file.isEmpty())
      {
        m_editor->setText(file);
        DefaultEditorChanged(file);
      }
    });

    connect(m_editor, &QLineEdit::editingFinished, this, [=] {
      DefaultEditorChanged(m_editor->text());
    });
    sublay->addWidget(m_editor);
    sublay->addWidget(btn);

    lay->addRow(tr("Default editor"), subw);
  }
  // ZOOM
  m_zoomSpinBox = new QSpinBox;
  m_zoomSpinBox->setMinimum(50);
  m_zoomSpinBox->setMaximum(300);

  connect(m_zoomSpinBox, SignalUtils::QSpinBox_valueChanged_int(), this, &View::zoomChanged);

  m_zoomSpinBox->setSuffix(tr("%"));

  lay->addRow(tr("Graphical Zoom (50% -- 300%)"), m_zoomSpinBox);

  // SLOT HEIGHT
  m_slotHeightBox = new QSpinBox;
  m_slotHeightBox->setMinimum(0);
  m_slotHeightBox->setMaximum(10000);

  connect(
      m_slotHeightBox,
      static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
      this,
      &View::SlotHeightChanged);

  lay->addRow(tr("Default Slot Height"), m_slotHeightBox);

  // Default duration
  m_defaultDur = new score::TimeSpinBox;
  connect(m_defaultDur, &score::TimeSpinBox::timeChanged, this, [=](const TimeVal& t) {
    DefaultDurationChanged(t);
  });
  lay->addRow(tr("New score duration"), m_defaultDur);

  m_sequence = new QCheckBox{tr("Auto-Sequence"), m_widg};
  connect(m_sequence, &QCheckBox::toggled, this, &View::AutoSequenceChanged);
  lay->addRow(m_sequence);

  SETTINGS_UI_TOGGLE_SETUP("Time Bar", TimeBar);
  SETTINGS_UI_TOGGLE_SETUP("Show musical metrics", MeasureBars);
  SETTINGS_UI_TOGGLE_SETUP("Magnetism on musical metrics", MagneticMeasures);
}

SETTINGS_UI_TOGGLE_IMPL(TimeBar)
SETTINGS_UI_TOGGLE_IMPL(MeasureBars)
SETTINGS_UI_TOGGLE_IMPL(MagneticMeasures)

void View::setSkin(const QString& val)
{
  if (val != m_skin->currentText())
  {
    int index = m_skin->findData(val);
    if (index != -1)
    {
      m_skin->setCurrentIndex(index);
    }
    else
    {
      m_skin->addItem(val, QVariant{val});
      m_skin->setCurrentIndex(m_skin->count()-1);
    }
  }
}

void View::setDefaultEditor(QString val)
{
  if (val != m_editor->text())
    m_editor->setText(val);
}
void View::setZoom(const int val)
{
  if (val != m_zoomSpinBox->value())
    m_zoomSpinBox->setValue(val);
}

void View::setDefaultDuration(const TimeVal& t)
{
  if (t != m_defaultDur->time())
    m_defaultDur->setTime(t);
}

void View::setSlotHeight(const double val)
{
  if (val != m_slotHeightBox->value())
    m_slotHeightBox->setValue(val);
}

void View::setAutoSequence(const bool val)
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

