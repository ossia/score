#include "Inspector.hpp"

#include <Inspector/InspectorLayout.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

namespace Gfx::Video
{
InspectorWidget::InspectorWidget(
    const Gfx::Video::Model& object, const score::DocumentContext& context,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
    , m_dispatcher{context.commandStack}
{
  auto lay = new Inspector::Layout{this};
  {
    auto edit = new QLineEdit{object.path(), this};
    lay->addRow(tr("Path"), edit);

    con(object, &Gfx::Video::Model::pathChanged, this, [edit](const QString& str) {
      if(str != edit->text())
        edit->setText(str);
    });
    connect(edit, &QLineEdit::editingFinished, this, [this, edit] {
      if(edit->text() != this->process().path())
        this->m_dispatcher.submit<ChangeVideo>(this->process(), edit->text());
    });
  }

  {
    auto cb = new QCheckBox{};
    cb->setChecked(!object.ignoreTempo());

    auto spin = new QDoubleSpinBox;
    spin->setRange(1., 500.);
    spin->setDecimals(1);
    spin->setValue(object.nativeTempo());

    auto combo = new QComboBox;
    combo->addItems(
        {tr("Original size"), tr("Expand (Black bars)"), tr("Expand (Fill)"),
         tr("Stretch")});
    combo->setCurrentIndex((int)object.scaleMode());

    auto playbackCombo = new QComboBox;
    playbackCombo->addItems({tr("Auto"), tr("Direct (seek)"), tr("Frame queue")});
    playbackCombo->setCurrentIndex((int)object.playbackMode());

    auto formatCombo = new QComboBox;
    formatCombo->addItems({tr("SDR"), tr("Passthrough"), tr("Linear"), tr("Normalized")});
    formatCombo->setCurrentIndex((int)object.outputFormat());

    auto tonemapCombo = new QComboBox;
    tonemapCombo->addItems({tr("Clamp"), tr("BT.2390"), tr("BT.2446"), tr("Reinhard"), tr("Hable"), tr("ACES2"), tr("AgX"), tr("PBR Neutral")});
    tonemapCombo->setCurrentIndex((int)object.tonemap());

    con(object, &Gfx::Video::Model::ignoreTempoChanged, this, [=](bool ignore) {
      if(cb->isChecked() != (!ignore))
        cb->setChecked(!ignore);
    });

    con(object, &Gfx::Video::Model::nativeTempoChanged, this, [=](double tempo) {
      if(spin->value() != tempo)
        spin->setValue(tempo);
    });
    con(object, &Gfx::Video::Model::scaleModeChanged, this,
        [=](score::gfx::ScaleMode sm) {
      int idx = (int)sm;
      if(combo->currentIndex() != idx)
        combo->setCurrentIndex(idx);
    });
    con(object, &Gfx::Video::Model::playbackModeChanged, this,
        [=](score::gfx::PlaybackMode pm) {
      int idx = (int)pm;
      if(playbackCombo->currentIndex() != idx)
        playbackCombo->setCurrentIndex(idx);
    });
    con(object, &Gfx::Video::Model::outputFormatChanged, this,
        [=](::Video::OutputFormat pm) {
      int idx = (int)pm;
      if(formatCombo->currentIndex() != idx)
        formatCombo->setCurrentIndex(idx);
    });
    con(object, &Gfx::Video::Model::tonemapChanged, this,
        [=](::Video::Tonemap pm) {
      int idx = (int)pm;
      if(tonemapCombo->currentIndex() != idx)
        tonemapCombo->setCurrentIndex(idx);
    });

    connect(cb, &QCheckBox::toggled, this, [this](bool t) {
      if((!t) != this->process().ignoreTempo())
      {
        this->m_dispatcher.submit<ChangeIgnoreTempo>(this->process(), !t);
      }
    });

    connect(
        spin, SignalUtils::SpinBox_valueChanged<QDoubleSpinBox>(), this,
        [this](double t) {
      if(t != this->process().nativeTempo())
      {
        this->m_dispatcher.submit<ChangeTempo>(this->process(), t);
      }
        });

    connect(
        combo, qOverload<int>(&QComboBox::currentIndexChanged), &object,
        [this](int idx) {
      auto new_mode = (score::gfx::ScaleMode)idx;
      if(new_mode != this->process().scaleMode())
      {
        this->m_dispatcher.submit<ChangeVideoScaleMode>(this->process(), new_mode);
      }
        });

    connect(
        playbackCombo, qOverload<int>(&QComboBox::currentIndexChanged), &object,
        [this](int idx) {
      auto new_mode = (score::gfx::PlaybackMode)idx;
      if(new_mode != this->process().playbackMode())
      {
        this->m_dispatcher.submit<ChangePlaybackMode>(this->process(), new_mode);
      }
        });

    connect(
        formatCombo, qOverload<int>(&QComboBox::currentIndexChanged), &object,
        [this](int idx) {
      auto new_mode = (::Video::OutputFormat)idx;
      if(new_mode != this->process().outputFormat())
      {
        this->m_dispatcher.submit<ChangeOutputFormat>(this->process(), new_mode);
      }
    });
    connect(
        tonemapCombo, qOverload<int>(&QComboBox::currentIndexChanged), &object,
        [this](int idx) {
      auto new_mode = (::Video::Tonemap)idx;
      if(new_mode != this->process().tonemap())
      {
        this->m_dispatcher.submit<ChangeTonemap>(this->process(), new_mode);
      }
    });

    lay->addRow(tr("Scale"), combo);
    lay->addRow(tr("Playback"), playbackCombo);
    lay->addRow(tr("Enable tempo"), cb);
    lay->addRow(tr("Tempo"), spin);
    lay->addRow(tr("Format"), formatCombo);
    lay->addRow(tr("Tonemap (HDR)"), tonemapCombo);
  }
}

InspectorWidget::~InspectorWidget() { }
}
