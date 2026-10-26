#include "AudioApplicationPlugin.hpp"

#include <QSignalBlocker>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Application/ScenarioActions.hpp>

#include <Audio/AudioDevice.hpp>
#include <Audio/AudioInterface.hpp>
#include <Audio/AudioTick.hpp>
#include <Audio/Settings/Model.hpp>

#include <score/model/Skin.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/detail/thread.hpp>
#include <ossia/audio/audio_engine.hpp>
#include <ossia/audio/audio_protocol.hpp>

#include <QTimer>
#include <QLabel>
#include <QToolBar>

SCORE_DECLARE_ACTION(RestartAudio, "Restart Audio", Common, QKeySequence::UnknownKey)
namespace Audio
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  startTimer(50);
}

void ApplicationPlugin::initialize()
{
  auto& set = context.settings<Audio::Settings::Model>();

  // First validate the current audio settings

  auto& engines = score::GUIAppContext().interfaces<Audio::AudioFactoryList>();

  if(auto dev = engines.get(set.getDriver()))
  {
    dev->initialize(set, this->context);
  }

  con(set, &Audio::Settings::Model::changed, this, &ApplicationPlugin::restart_engine,
      Qt::QueuedConnection);

#if !defined(__EMSCRIPTEN__)
  ossia::lock_process_memory(set.getLockMemory());
  con(set, &Audio::Settings::Model::LockMemoryChanged, this,
      [](bool lock) { ossia::lock_process_memory(lock); });
#endif
}

ApplicationPlugin::~ApplicationPlugin() { }

void ApplicationPlugin::on_closeDocument(score::Document& old)
{
  // The engine does not depend on the document: keep it, parked on the
  // document-independent pause tick (set_tick() waits for the audio thread to
  // pick it up, so nothing of the closing document is referenced afterwards).
  //
  // Stopping it here used to leave the audio dead: closing a document that is
  // not the current one changes no document, so nothing restarted the engine;
  // and closing the current one recreated the driver, which is slow and
  // fragile (ASIO, WASAPI...). On WebAssembly the WebAudio worklet cannot be
  // recreated at all.
  //
  // The closing document is still listed here: it is the last one when no
  // other remains.
  const auto& docs = context.docManager.documents();
  const bool last = docs.size() <= 1;
  if(audio)
  {
    audio->set_tick(Audio::makePauseTick(this->context));
#if !defined(__EMSCRIPTEN__)
    if(last)
      stop_engine();
#endif
  }
}

void ApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newdoc)
{
  if(!newdoc)
    return;

  if(!audio)
  {
    restart_engine();
    return;
  }

  // The engine runs on: bind the document's audio device to it
  rebind_engine(*newdoc);
}

void ApplicationPlugin::rebind_engine(score::Document& doc)
{
  auto dev = (Dataflow::AudioDevice*)doc.context()
                 .plugin<Explorer::DeviceDocumentPlugin>()
                 .list()
                 .audioDevice();
  if(dev)
    dev->reconnect();
  if(audio)
    audio->set_tick(Audio::makePauseTick(this->context));
  showVolume(doc);
}

void ApplicationPlugin::showVolume(score::Document& doc)
{
  if(!m_volume)
    return;
  auto dev = (Dataflow::AudioDevice*)doc.context()
                 .plugin<Explorer::DeviceDocumentPlugin>()
                 .list()
                 .audioDevice();
  if(!dev || !dev->getDevice())
    return;
  auto node = ossia::net::find_node(dev->getDevice()->get_root_node(), "/out/main");
  if(!node || !node->get_parameter())
    return;

  // Display only: the gain already has this value.
  QSignalBlocker block{m_volume};
  m_volume->setValue(ossia::convert<double>(node->get_parameter()->value()));
  m_volume->update();
}

void ApplicationPlugin::timerEvent(QTimerEvent*)
{
  if(audio)
  {
    audio->gc();
    if(audio->failed() && !m_updating_audio)
    {
      stop_engine();
      // Its stream died, so no callback will mark it stopped for the
      // reaper; stop() has closed the stream and joined its thread.
      previous_audio.pop_back();
      score::warning(
          context.documentTabWidget, tr("Audio Error"),
          tr("The audio device stopped working, or cannot keep up at this buffer "
             "size. Check that it is connected and not used by another program, or "
             "choose a larger buffer size, then restart the audio engine."));
    }
  }

  reap_previous_engines();
  updateXruns();
}

void ApplicationPlugin::updateXruns()
{
  if(!m_xruns)
    return;
  const int64_t n = audio ? audio->xruns.load(std::memory_order_relaxed) : 0;
  if(n == m_shownXruns)
    return;
  m_shownXruns = n;
  m_xrunsAction->setVisible(n > 0);
  m_xruns->setText(n == 1 ? tr("1 dropout") : tr("%1 dropouts").arg(n));
  if(audio)
    m_xruns->setToolTip(
        tr("Buffer under- or overruns reported by the audio driver since the engine "
           "started.\nPeriod: %1 frames at %2 Hz.\nLatency: %3 frames in, %4 frames "
           "out.\nA larger buffer size makes them less likely.")
            .arg(audio->effective_buffer_size)
            .arg(audio->effective_sample_rate)
            .arg(audio->effective_input_latency.load())
            .arg(audio->effective_output_latency.load()));
}

void ApplicationPlugin::reap_previous_engines()
{
  for(auto it = previous_audio.begin(); it != previous_audio.end();)
  {
    auto& engine = **it;
    engine.gc();
    // stop_received is set from inside the audio callback, which still uses
    // the engine afterwards: only the driver being down makes it reapable.
    if(engine.stop_received && !engine.running())
    {
      it = previous_audio.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

score::GUIElements ApplicationPlugin::makeGUIElements()
{
  GUIElements e;

  auto& toolbars = e.toolbars;

  // The toolbar with the volume control
  m_audioEngineAct = new QAction{tr("Restart Audio"), this};
  m_audioEngineAct->setCheckable(true);
  m_audioEngineAct->setChecked(bool(audio));
  score::setHelp(m_audioEngineAct, "Restart the audio engine");

  setIcons(
      m_audioEngineAct, QStringLiteral(":/icons/engine_on.png"),
      QStringLiteral(":/icons/engine_off.png"),
      QStringLiteral(":/icons/engine_disabled.png"), false);
  {
    auto bar = new QToolBar(tr("Volume"));
    auto sl = new score::VolumeSlider{bar};
    m_volume = sl;
    // Pinned, or the toolbar stretches it across whatever room is left.
    score::onSkinChange(sl, [sl] {
      sl->setFixedSize(score::scaledPixels(100), sl->skinExtent());
    });
    sl->setValue(0.5);
    score::setHelp(sl, "Change the master volume");
    bar->addWidget(sl);
    bar->addAction(m_audioEngineAct);
    m_xruns = new QLabel{bar};
    // A toolbar shows and hides its widgets through their actions.
    m_xrunsAction = bar->addWidget(m_xruns);
    m_xrunsAction->setVisible(false);
    connect(sl, &score::VolumeSlider::valueChanged, this, [this](double v) {
      if(!this->audio)
        return;

      auto doc = context.currentDocument();
      if(!doc)
        return;
      auto dev = (Dataflow::AudioDevice*)doc->plugin<Explorer::DeviceDocumentPlugin>()
                     .list()
                     .audioDevice();
      if(!dev)
        return;
      auto p = dev->getProtocol();
      if(!p)
        return;

      auto root = ossia::net::find_node(dev->getDevice()->get_root_node(), "/out/main");
      if(root)
      {
        if(auto p = root->get_parameter())
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

  connect(
      m_audioEngineAct, &QAction::triggered, this, &ApplicationPlugin::restart_engine);

  return e;
}

void ApplicationPlugin::restart_engine()
try
{
  if(m_updating_audio)
    return;

#if defined(__EMSCRIPTEN__)
  if(!audio)
  {
    // Defer the first start to a clean stack depth: the WebAudio backend blocks
    // (emscripten_sleep / ASYNCIFY) waiting for the AudioWorklet, which deadlocks
    // if run synchronously while nested inside document loading.
    QTimer::singleShot(0, this, [this] { start_engine(); });
  }
  else
  {
    // The worklet is a page-lifetime singleton and cannot be recreated, so a
    // new/changed document rebinds its audio device to the running engine rather
    // than tearing it down.
    QTimer::singleShot(0, this, [this] {
      if(auto doc = this->currentDocument())
      {
        auto dev = (Dataflow::AudioDevice*)doc->context()
                       .plugin<Explorer::DeviceDocumentPlugin>()
                       .list()
                       .audioDevice();
        if(dev)
          dev->reconnect();
        if(audio)
          audio->set_tick(Audio::makePauseTick(this->context));
      }
    });
  }
#else
  stop_engine();
  // A device can only be opened once: the engine that just stopped has to
  // release it before the new one opens it.
  reap_previous_engines();
  start_engine();
#endif
}
catch(std::exception& e)
{
  score::warning(
      context.documentTabWidget, tr("Audio Error"),
      tr("Warning: error while restarting the audio engine:\n%1").arg(e.what()));
}
catch(...)
{
  score::warning(
      context.documentTabWidget, tr("Audio Error"),
      tr("Warning: audio engine stuck. "
         "Operation aborted. "
         "Check the audio settings."));
}

void ApplicationPlugin::stop_engine()
{
  if(audio)
  {
    // The spans the protocols hold point into this engine's buffers, and the
    // callback rebinds them every tick: it has to be parked before they go.
    audio->stop();

    for(auto d : context.docManager.documents())
    {
      auto dev = (Dataflow::AudioDevice*)d->context()
                     .plugin<Explorer::DeviceDocumentPlugin>()
                     .list()
                     .audioDevice();
      if(dev)
      {
        if(auto proto = dev->getProtocol())
        {
          proto->stop();
        }
      }
    }

    previous_audio.push_back(std::move(audio));
    audio.reset();
    if(m_audioEngineAct)
      m_audioEngineAct->setChecked(false);
  }
}

void ApplicationPlugin::start_engine()
{
  if(auto doc = this->currentDocument())
  {
    auto dev = (Dataflow::AudioDevice*)doc->context()
                   .plugin<Explorer::DeviceDocumentPlugin>()
                   .list()
                   .audioDevice();
    if(!dev)
      return;

    auto& set = this->context.settings<Audio::Settings::Model>();
    auto& engines = score::GUIAppContext().interfaces<Audio::AudioFactoryList>();

    if(auto dev = engines.get(set.getDriver()))
    {
      try
      {
        SCORE_ASSERT(!audio);
        audio = dev->make_engine(set, this->context);
        if(!audio)
          throw std::runtime_error{"the driver could not create an audio engine"};

        struct updating_guard
        {
          bool& flag;
          updating_guard(bool& f)
              : flag{f}
          {
            flag = true;
          }
          ~updating_guard() { flag = false; }
        } guard{m_updating_audio};

        auto bs = audio->effective_buffer_size;
        if(bs <= 0)
          bs = set.getBufferSize();
        if(bs <= 0)
          bs = 512;
        auto rate = audio->effective_sample_rate;
        if(rate <= 0)
          rate = set.getRate();
        if(rate <= 0)
          rate = 44100;
        set.setBufferSize(bs);
        set.setRate(rate);
      }
      catch(std::exception& e)
      {
        score::warning(
            nullptr, tr("Audio error"),
            tr("The audio settings could not be applied:\n%1\n\nPlease change "
               "them.")
                .arg(QString::fromStdString(e.what())));
      }
      catch(...)
      {
        score::warning(
            nullptr, tr("Audio error"),
            tr("The desired audio settings could not be applied.\nPlease "
               "change "
               "them."));
      }
    }

    if(m_audioEngineAct)
      m_audioEngineAct->setChecked(bool(audio));

    dev->reconnect();
    if(audio)
    {
      audio->set_tick(Audio::makePauseTick(this->context));
    }
    if(auto doc = this->currentDocument())
      showVolume(*doc);
  }
}

}
