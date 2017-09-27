#include "AudioInspector.hpp"
#include <Media/Commands/ChangeAudioFile.hpp>
#include <score/document/DocumentContext.hpp>

#include <QFormLayout>
#include <QLineEdit>
namespace Media
{
namespace Sound
{
InspectorWidget::InspectorWidget(
    const Sound::ProcessModel &object,
    const score::DocumentContext &doc,
    QWidget *parent):
  InspectorWidgetDelegate_T {object, parent},
  m_dispatcher{doc.commandStack}
, m_edit{object.file().name(), this}
, m_start{this}
, m_upmix{this}
{
  m_start.setValue(object.startChannel());
  m_upmix.setValue(object.upmixChannels());
  m_start.setRange(0, 512);
  m_upmix.setRange(0, 512);

  setObjectName("SoundInspectorWidget");

  auto lay = new QFormLayout;

  con(process(), &Sound::ProcessModel::startChannelChanged,
      this, [&] {
    m_start.setValue(object.startChannel());
  });
  con(process(), &Sound::ProcessModel::upmixChannelsChanged,
      this, [&] {
    m_upmix.setValue(object.upmixChannels());
  });
  con(process(), &Sound::ProcessModel::fileChanged,
      this, [&] {
    m_edit.setText(object.file().name());
  });

  con(m_edit, &QLineEdit::editingFinished,
      this, [&] () {
    m_dispatcher.submitCommand(new Commands::ChangeAudioFile(object, m_edit.text()));
  });
  con(m_start, &QSpinBox::editingFinished,
      this, [&] () {
    m_dispatcher.submitCommand(new Commands::ChangeStart(object, m_start.value()));
  });
  con(m_upmix, &QSpinBox::editingFinished,
      this, [&] () {
    m_dispatcher.submitCommand(new Commands::ChangeUpmix(object, m_upmix.value()));
  });

  lay->addRow(tr("Path"), &m_edit);
  lay->addRow(tr("Start channel"), &m_start);
  lay->addRow(tr("Upmix channels"), &m_upmix);
  this->setLayout(lay);
}
}


namespace Input
{
InspectorWidget::InspectorWidget(
    const Input::ProcessModel &object,
    const score::DocumentContext &doc,
    QWidget *parent):
  InspectorWidgetDelegate_T {object, parent},
  m_dispatcher{doc.commandStack}
, m_start{this}
, m_count{this}
{;
  m_start.setValue(object.startChannel());
  m_count.setValue(object.numChannel());
  m_start.setRange(0, 512);
  m_count.setRange(0, 512);

  setObjectName("InputInspectorWidget");

  auto lay = new QFormLayout;

  con(process(), &Input::ProcessModel::startChannelChanged,
      this, [&] {
    m_start.setValue(object.startChannel());
  });
  con(process(), &Input::ProcessModel::numChannelChanged,
      this, [&] {
    m_count.setValue(object.numChannel());
  });

  con(m_start, &QSpinBox::editingFinished,
      this, [&] () {
    m_dispatcher.submitCommand(new Commands::ChangeInputStart(object, m_start.value()));
  });
  con(m_count, &QSpinBox::editingFinished,
      this, [&] () {
    m_dispatcher.submitCommand(new Commands::ChangeInputNum(object, m_count.value()));
  });

  lay->addRow(tr("Start channel"), &m_start);
  lay->addRow(tr("Num channels"), &m_count);
  this->setLayout(lay);
}

}

}
