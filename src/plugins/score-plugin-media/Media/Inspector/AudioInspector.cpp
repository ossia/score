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

  setObjectName("SoundInspectorWidget");

  auto lay = new QFormLayout;

  ::bind(process(), Sound::ProcessModel::p_startChannel{}, this, [&](int v) {
    m_start.setValue(v);
  });
  ::bind(process(), Sound::ProcessModel::p_upmixChannels{}, this, [&](int v) {
    m_upmix.setValue(v);
  });
  con(process(), &Sound::ProcessModel::fileChanged, this, [&] {
    m_edit.setText(object.file()->originalFile());
  });

  con(m_edit, &QLineEdit::editingFinished, this, [&]() {
    m_dispatcher.submit(new ChangeAudioFile(object, m_edit.text()));
  });
  con(m_start, &QSpinBox::editingFinished, this, [&]() {
    m_dispatcher.submit(new ChangeStart(object, m_start.value()));
  });
  con(m_upmix, &QSpinBox::editingFinished, this, [&]() {
    m_dispatcher.submit(new ChangeUpmix(object, m_upmix.value()));
  });

  lay->addRow(tr("Path"), &m_edit);
  lay->addRow(tr("Start channel"), &m_start);
  lay->addRow(tr("Upmix channels"), &m_upmix);
  this->setLayout(lay);
}
}
}
