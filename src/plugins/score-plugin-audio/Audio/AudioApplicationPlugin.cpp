#include "AudioApplicationPlugin.hpp"
#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/actions/ActionManager.hpp>
#include <core/application/ApplicationSettings.hpp>

#include <QMessageBox>
#include <QToolBar>

SCORE_DECLARE_ACTION(
    RestartAudio,
    "Restart Audio",
    Common,
    QKeySequence::UnknownKey)
namespace Audio
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
  : score::GUIApplicationPlugin{ctx}
{
  if (ctx.applicationSettings.gui)
  {
    auto& stop_action = ctx.actions.action<Actions::Stop>();
    connect(
          stop_action.action(),
          &QAction::triggered,
          this,
          &ApplicationPlugin::on_stop,
          Qt::QueuedConnection);
  }
}

void ApplicationPlugin::initialize()
{
  auto& set = context.settings<Audio::Settings::Model>();

  con(set,
      &Audio::Settings::Model::changed,
      this,
      &ApplicationPlugin::restart_engine,
      Qt::QueuedConnection);

  try
  {
    setup_engine();
  }
  catch (...)
  {
  }
}

ApplicationPlugin::~ApplicationPlugin()
{
}

void ApplicationPlugin::on_stop()
{
  if (audio)
  {
    audio->reload(nullptr);
  }
}

void ApplicationPlugin::on_documentChanged(
            score::Document *olddoc,
            score::Document *newdoc)
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
    sl->setMaximumSize(100, 20);
    sl->setValue(0.5);
    sl->setStatusTip("Change the master volume");
    bar->addWidget(sl);
    bar->addAction(m_audioEngineAct);
    connect(sl, &score::VolumeSlider::valueChanged, this, [=](double v) {
      if(!this->audio)
        return;
      if(!this->audio->protocol)
        return;
      ossia::audio_protocol* p = this->audio->protocol;
      auto& dev = p->get_device();

      auto root
          = ossia::net::find_node(dev.get_root_node(), "/out/main");
      if (root)
      {
        if (auto p = root->get_parameter())
        {
          auto audio_p = static_cast<ossia::audio_parameter*>(p);
          audio_p->push_value(v);
        }
      }
    });

    toolbars.emplace_back(
          bar, StringKey<score::Toolbar>("Audio"), Qt::BottomToolBarArea, 400);
  }

  e.actions.container.reserve(2);
  e.actions.add<Actions::RestartAudio>(m_audioEngineAct);

  connect(m_audioEngineAct, &QAction::triggered, this, [=](bool) {
    if (audio)
    {
      audio->stop();
      audio.reset();
    }
    else
    {
      setup_engine();
    }
    m_audioEngineAct->setChecked(bool(audio));

    if (auto doc = currentDocument())
    {
      auto dev = doc->context()
          .plugin<Explorer::DeviceDocumentPlugin>()
          .list()
          .audioDevice();
      if (!dev)
        return;
      dev->reconnect();
    }
  });

  return e;
}
void ApplicationPlugin::restart_engine()
{
  if (m_updating_audio)
    return;

  if (auto doc = this->currentDocument())
  {
    auto dev = doc->context()
        .plugin<Explorer::DeviceDocumentPlugin>()
        .list()
        .audioDevice();
    if (!dev)
      return;
    if (audio)
      audio->stop();

    setup_engine();

    dev->reconnect();
  }
}


void ApplicationPlugin::setup_engine()
{
  auto& ctx = score::AppContext();
  auto& set = ctx.settings<Audio::Settings::Model>();
  auto& engines = score::GUIAppContext().interfaces<Audio::AudioFactoryList>();

  audio.reset();
  if (auto dev = engines.get(set.getDriver()))
  {
    try
    {
      audio = dev->make_engine(set, ctx);
      if(!audio)
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
      /*
      QMessageBox::warning(
            nullptr,
            tr("Audio error"),
            tr("The desired audio settings could not be applied.\nPlease change "
               "them."));
               */
    }
  }

  if (m_audioEngineAct)
    m_audioEngineAct->setChecked(bool(audio));
}

}
