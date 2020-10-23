#include "AudioInspector.hpp"

#include <Media/Commands/ChangeAudioFile.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <QFormLayout>
#include <QLineEdit>
namespace Media
{
namespace Sound
{
InspectorWidget::InspectorWidget(
    const Sound::ProcessModel& object,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}
    , m_dispatcher{doc.commandStack}
    , m_edit{object.file()->originalFile(), this}
    , m_start{this}
    , m_upmix{this}
{
  m_start.setRange(0, 512);
  m_upmix.setRange(0, 512);
  m_mode.addItems({tr("Raw"), tr("Timestretch"), tr("Timestretch (percussive)"), tr("Repitch")});

  setObjectName("SoundInspectorWidget");

  auto lay = new QFormLayout;

  ::bind(process(), Sound::ProcessModel::p_startChannel{}, this, [&](int v) {
    if (m_start.value() != v)
      m_start.setValue(v);
  });
  ::bind(process(), Sound::ProcessModel::p_upmixChannels{}, this, [&](int v) {
    if (m_upmix.value() != v)
      m_upmix.setValue(v);
  });
  ::bind(process(), Sound::ProcessModel::p_stretchMode{}, this, [&](ossia::audio_stretch_mode v) {
    if (m_mode.currentIndex() != (int)v)
      m_mode.setCurrentIndex((int)v);
  });
  ::bind(process(), Sound::ProcessModel::p_nativeTempo{}, this, [&](double t) {
    if (m_tempo.value() != t)
      m_tempo.setValue(t);
  });

  con(process(), &Sound::ProcessModel::fileChanged, this, [&] {
    m_edit.setText(object.file()->originalFile());
  });

  con(m_edit, &QLineEdit::editingFinished, this, [&]() {
    m_dispatcher.submit(new ChangeAudioFile(object, m_edit.text(), this->m_dispatcher.stack().context()));
  });
  con(m_start, &QSpinBox::editingFinished, this, [&]() {
    if (m_start.value() != process().startChannel())
      m_dispatcher.submit(new ChangeStart(object, m_start.value()));
  });
  con(m_upmix, &QSpinBox::editingFinished, this, [&]() {
    if (m_upmix.value() != process().upmixChannels())
      m_dispatcher.submit(new ChangeUpmix(object, m_upmix.value()));
  });
  con(m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this, [&](int idx) {
    if (idx != (int)process().stretchMode())
      m_dispatcher.submit(new ChangeStretchMode(object, (ossia::audio_stretch_mode)idx));
  });
  con(m_tempo, &score::SpinBox<double>::editingFinished, this, [&]() {
    if (m_tempo.value() != process().nativeTempo())
      m_dispatcher.submit(new ChangeTempo(object, m_tempo.value()));
  });

  lay->addRow(tr("Path"), &m_edit);
  lay->addRow(tr("Stretch mode"), &m_mode);
  lay->addRow(tr("Start channel"), &m_start);
  lay->addRow(tr("Upmix channels"), &m_upmix);
  lay->addRow(tr("File tempo"), &m_tempo);
  this->setLayout(lay);
}
}
}
