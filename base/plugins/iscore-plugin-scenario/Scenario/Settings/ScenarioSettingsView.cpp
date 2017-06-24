#include "ScenarioSettingsView.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QJsonDocument>
#include <QLineEdit>
#include <QDialog>
#include <QtColorWidgets/ColorWheel>
#include <QFileDialog>
#include <iscore/widgets/SignalUtils.hpp>

namespace Scenario
{
namespace Settings
{

class ThemeDialog: public QDialog
{
public:
    QHBoxLayout layout;
    QVBoxLayout sublay;
    QListWidget list;
    QLineEdit hexa;
    QLineEdit rgb;
    QPushButton save{tr("Save")};
    color_widgets::ColorWheel wheel;
    ThemeDialog(QWidget* p): QDialog{p}
    {
        layout.addWidget(&list);
        layout.addLayout(&sublay);
        sublay.addWidget(&wheel);
        sublay.addWidget(&hexa);
        sublay.addWidget(&rgb);
        sublay.addWidget(&save);
        this->setLayout(&layout);
        iscore::Skin& s = iscore::Skin::instance();
        for(auto& col : s.getColors())
        {
            QPixmap p{16, 16};
            p.fill(col.first);
            list.addItem(new QListWidgetItem(p, col.second));
        }

        connect(&list, &QListWidget::currentItemChanged, this,
                [&] (QListWidgetItem* cur, auto prev) {
            if(cur)
                wheel.setColor(s.fromString(cur->text())->color());
        });

        connect(&wheel, &color_widgets::ColorWheel::colorChanged,
                this, [&] (QColor c) {
            if(list.currentItem())
            {
                s.fromString(list.currentItem()->text())->setColor(c);
                QPixmap p{16, 16};
                p.fill(c);
                list.currentItem()->setIcon(p);
                s.changed();
                hexa.setText(c.name(QColor::HexRgb));
                rgb.setText(QString("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue()));
            }
        });

        connect(&save, &QPushButton::clicked, this, [] {
            auto f = QFileDialog::getSaveFileName(nullptr, tr("Skin"), "", tr("*.json"));
            if(f.isEmpty())
                return;
            QFile fl{f};
            fl.open(QIODevice::WriteOnly);
            if(!fl.isOpen())
                return;

            QJsonObject obj;
            for(auto& col : iscore::Skin::instance().getColors())
            {
                obj.insert(col.second, toJsonValue(col.first));
            }

            QJsonDocument doc;
            doc.setObject(obj);
            fl.write(doc.toJson());
        });
    }
};

View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;
  m_widg->setLayout(lay);

  // SKIN
  m_skin = new QComboBox;
  m_skin->addItems({"Default", "IEEE"});
  lay->addRow(tr("Skin"), m_skin);
  auto es = new QPushButton{tr("Edit skin")};
  connect(es, &QPushButton::clicked, this, []
  {
      ThemeDialog d{nullptr};
      d.exec();
  });
  auto ls = new QPushButton{tr("Load skin")};
  connect(ls, &QPushButton::clicked, this, [=] {
      auto f = QFileDialog::getOpenFileName(nullptr, tr("Skin"), tr("*.json"));
      emit skinChanged(f);
  });
  lay->addWidget(ls);
  lay->addWidget(es);

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
      [=](const QTime& t) { emit defaultDurationChanged(TimeVal{t}); });
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

void View::setDefaultDuration(const TimeVal& t)
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
