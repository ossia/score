#include "AudioApplicationPlugin.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/ExecutionAction.hpp>
#include <Scenario/Application/ScenarioActions.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/audio/audio_protocol.hpp>

#include <QToolBar>

#include <Audio/AudioDevice.hpp>
#include <Audio/AudioTick.hpp>
#include <Audio/AudioInterface.hpp>
#include <Audio/AudioPreviewExecutor.hpp>
#include <Audio/Settings/Model.hpp>

SCORE_DECLARE_ACTION(RestartAudio, "Restart Audio", Common, QKeySequence::UnknownKey)
namespace Audio
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
}

void ApplicationPlugin::initialize()
{
  auto& set = context.settings<Audio::Settings::Model>();

  con(set,
      &Audio::Settings::Model::changed,
      this,
      &ApplicationPlugin::restart_engine,
      Qt::QueuedConnection);
}

ApplicationPlugin::~ApplicationPlugin() { }

void ApplicationPlugin::on_documentChanged(score::Document* olddoc, score::Document* newdoc)
{
  restart_engine();
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{

  GUIElements e;

  auto& toolbars = e.toolbars;

  // The toolbar with the volume control
  m_audioEngineAct = new QAction{tr("Restart Audio"), this};
  m_audioEngineAct->setCheckable(true);
  m_audioEngineAct->setChecked(bool(audio));
  m_audioEngineAct->setStatusTip("Restart the audio engine");

  setIcons(
      m_audioEngineAct,
      QStringLiteral(":/icons/engine_on.png"),
      QStringLiteral(":/icons/engine_off.png"),
      QStringLiteral(":/icons/engine_disabled.png"),
      false);
  {
    auto bar = new QToolBar(tr("Volume"));
    auto sl = new score::VolumeSlider{bar};
    sl->setFixedSize(100, 20);
    sl->setValue(0.5);
    sl->setStatusTip("Change the master volume");
    bar->addWidget(sl);
    bar->addAction(m_audioEngineAct);
    connect(sl, &score::VolumeSlider::valueChanged, this, [=](double v) {
      if (!this->audio)
        return;
      if (!this->audio->protocol)
        return;
      ossia::audio_protocol* p = this->audio->protocol;
      auto& dev = p->get_device();

      auto root = ossia::net::find_node(dev.get_root_node(), "/out/main");
      if (root)
      {
        if (auto p = root->get_parameter())
        {
          auto audio_p = static_cast<ossia::audio_parameter*>(p);
          audio_p->push_value(v);
        }
      }
    });

    toolbars.emplace_back(bar, StringKey<score::Toolbar>("Audio"), Qt::BottomToolBarArea, 400);
  }

  e.actions.container.reserve(2);
  e.actions.add<Actions::RestartAudio>(m_audioEngineAct);

  connect(m_audioEngineAct, &QAction::triggered, this, &ApplicationPlugin::restart_engine);

  return e;
}

void ApplicationPlugin::restart_engine()
try
{
  if (m_updating_audio)
    return;
  auto& preview = AudioPreviewExecutor::instance();
  preview.audio = nullptr;

  if (audio)
  {
    for(auto d : context.docManager.documents())
    {
      auto dev = (Dataflow::AudioDevice*) d->context().plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice();
      if(dev)
      {
        if(auto proto = dev->getProtocol())
        {
          proto->stop();
        }
      }
    }

    audio->stop();

    audio.reset();
  }

  if (auto doc = this->currentDocument())
  {
    auto dev = (Dataflow::AudioDevice*) doc->context().plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice();
    if (!dev)
      return;
    setup_engine();

    if(m_audioEngineAct)
      m_audioEngineAct->setChecked(bool(audio));

    auto& preview = AudioPreviewExecutor::instance();
    preview.audio = nullptr;
    dev->reconnect();
    if (audio)
    {
      preview.audio = audio->protocol;
      audio->set_tick(Audio::makePauseTick(this->context));
    }
    else
    {
      preview.audio = nullptr;
    }
  }
}
catch (...)
{
  score::warning(
      context.documentTabWidget,
      tr("Audio Error"),
      tr("Warning: audio engine stuck. "
         "Operation aborted. "
         "Check the audio settings."));
}

void ApplicationPlugin::setup_engine()
{
  auto& set = this->context.settings<Audio::Settings::Model>();
  auto& engines = score::GUIAppContext().interfaces<Audio::AudioFactoryList>();

  auto& preview = AudioPreviewExecutor::instance();
  preview.audio = nullptr;
  audio.reset();
  if (auto dev = engines.get(set.getDriver()))
  {
    try
    {
      audio = dev->make_engine(set, this->context);
      if (!audio)
        throw std::runtime_error{""};

      m_updating_audio = true;
      auto bs = audio->effective_buffer_size;
      auto rate = audio->effective_sample_rate;
      set.setBufferSize(bs <= 0 ? 512 : bs);
      set.setRate(rate <= 0 ? 44100 : rate);

      m_updating_audio = false;
    }
    catch (...)
    {
      score::warning(
          nullptr,
          tr("Audio error"),
          tr("The desired audio settings could not be applied.\nPlease change "
             "them."));
    }
  }

  if (m_audioEngineAct)
    m_audioEngineAct->setChecked(bool(audio));
  if (audio)
  {
    preview.audio = audio->protocol;
  }
}

}
