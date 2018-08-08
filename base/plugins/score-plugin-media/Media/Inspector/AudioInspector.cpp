#include "AudioInspector.hpp"

#include <Media/Commands/ChangeAudioFile.hpp>
#include <QFormLayout>
#include <QLineEdit>
#include <score/document/DocumentContext.hpp>
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
    , m_edit{object.file().path(), this}
    , m_start{this}
    , m_upmix{this}
    , m_startOffset{this}
{
  m_start.setRange(0, 512);
  m_upmix.setRange(0, 512);
  m_startOffset.setRange(0, INT_MAX);

  setObjectName("SoundInspectorWidget");

  auto lay = new QFormLayout;

  ::bind(process(), Sound::ProcessModel::p_startChannel{}, this,
       [&] (int v) { m_start.setValue(v); });
  ::bind(process(), Sound::ProcessModel::p_upmixChannels{}, this,
      [&] (int v) { m_upmix.setValue(v); });
  ::bind(process(), Sound::ProcessModel::p_startOffset{}, this,
      [&] (qint32 v) { m_startOffset.setValue(v); });
  con(process(), &Sound::ProcessModel::fileChanged, this,
      [&] { m_edit.setText(object.file().path()); });

  con(m_edit, &QLineEdit::editingFinished, this, [&]() {
    m_dispatcher.submitCommand(
        new ChangeAudioFile(object, m_edit.text()));
  });
  con(m_start, &QSpinBox::editingFinished, this, [&]() {
    m_dispatcher.submitCommand(
        new ChangeStart(object, m_start.value()));
  });
  con(m_upmix, &QSpinBox::editingFinished, this, [&]() {
    m_dispatcher.submitCommand(
        new ChangeUpmix(object, m_upmix.value()));
  });
  con(m_startOffset, &QSpinBox::editingFinished, this, [&]() {
    m_dispatcher.submitCommand(
          new ChangeStartOffset(object, m_startOffset.value()));
  });

  lay->addRow(tr("Path"), &m_edit);
  lay->addRow(tr("Start channel"), &m_start);
  lay->addRow(tr("Upmix channels"), &m_upmix);
  lay->addRow(tr("Start offset (samples)"), &m_startOffset);
  this->setLayout(lay);
}
}

}
