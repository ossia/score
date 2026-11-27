#include "Metronome.hpp"

#include <Process/Execution/PortTap.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>

#include <Audio/AudioDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Tempo/TempoProcess.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Media/Metro/MetroModel.hpp>
#include <Recording/Engine/CountIn.hpp>
#include <Recording/Settings/RecordingSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <algorithm>
#include <cmath>

namespace Recording
{
namespace
{
CountInHold* g_hold{};

Media::Metro::sound_set_ptr soundsOf(int kind, int rate)
{
  using namespace Media::Metro;
  if(kind == Classic)
    if(auto set = Model::classicSounds(rate))
      return set;
  return make_builtin_sounds(sound_kind(kind == Classic ? Click : kind), rate);
}

int soundOf(const Settings::Model& s)
{
  const QStringList sounds = Settings::ClickSounds{};
  return std::max(0, int(sounds.indexOf(s.getClickSound())));
}
}

CountInHold::CountInHold()
{
  g_hold = this;
}

CountInHold::~CountInHold()
{
  if(g_hold == this)
    g_hold = nullptr;
}

CountInHold* CountInHold::instance() noexcept
{
  return g_hold;
}

void CountInHold::arm(Count c)
{
  m_count = std::move(c);
  m_started = false;
  m_holding = false;
  m_pos = 0;
  m_clicked.store(0, std::memory_order_relaxed);
  m_armed.store(m_count.beats > 0 && m_count.beatFrames > 0, std::memory_order_release);
}

void CountInHold::cancel() noexcept
{
  m_armed.store(false, std::memory_order_release);
}

void CountInHold::mix(
    const Media::Metro::click_sound& snd, int64_t from, const ossia::audio_tick_state& st,
    int64_t at, int64_t n) const noexcept
{
  const auto& ch = m_count.channels;
  for(std::size_t c = 0; c < ch.size() && c < 2; c++)
  {
    const int out = ch[c];
    if(out < 0 || out >= st.n_out)
      continue;
    const auto& src = (c == 1 && !snd.right.empty()) ? snd.right : snd.left;
    float* dst = st.outputs[out] + at;
    for(int64_t i = 0; i < n; i++)
      dst[i] += src[from + i] * m_count.volume;
  }
}

void CountInHold::startTick(const ossia::audio_tick_state& st)
{
  m_holding = false;
  if(!m_armed.load(std::memory_order_acquire))
    return;
  const int64_t frames = int64_t(st.frames);
  if(frames <= 0)
    return;
  if(!m_started)
  {
    // Whole buffers: the last one ends a beat after the last click
    const int64_t total = m_count.beatFrames * m_count.beats;
    const int64_t buffers = (total + frames - 1) / frames;
    m_end = buffers * frames;
    m_lead = m_end - total;
    m_pos = 0;
    m_started = true;
  }

  m_holding = true;
  if(const auto* set = m_count.sounds.get())
  {
    for(int k = 0; k < m_count.beats; k++)
    {
      const int64_t start = m_lead + k * m_count.beatFrames;
      const bool first = m_count.beatsPerBar > 0 && k % m_count.beatsPerBar == 0;
      const auto& snd = (first && m_count.accent) ? set->accent : set->beat;
      const auto& s = snd.empty() ? set->accent : snd;
      const int64_t len = s.frames();
      // Its part in this buffer
      const int64_t a = std::max(start, m_pos);
      const int64_t b = std::min(start + len, m_pos + frames);
      if(a >= b)
        continue;
      if(start >= m_pos && start < m_pos + frames)
        m_clicked.fetch_add(1, std::memory_order_relaxed);
      mix(s, a - start, st, a - m_pos, b - a);
    }
  }
  m_pos += frames;
  if(m_pos >= m_end)
    m_armed.store(false, std::memory_order_release);
}

Metronome::Metronome(ApplicationPlugin& plug)
    : m_plug{plug}
{
  const auto& app = score::GUIAppContext();
  auto& set = app.settings<Settings::Model>();
  m_on->store(set.getMetronome());
  connect(&set, &Settings::Model::MetronomeChanged, this, [this](bool b) { m_on->store(b); });

  auto& exec = app.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();
  connect(&exec, &Execution::ExecutionController::aboutToPlay, this, &Metronome::aboutToPlay);
  connect(&exec, &Execution::ExecutionController::playStarted, this, &Metronome::started);
  connect(&exec, &Execution::ExecutionController::playStopped, this, &Metronome::stopped);
}

Metronome::~Metronome() = default;

bool Metronome::enabled() const noexcept
{
  return m_on->load();
}

int Metronome::beats() const noexcept
{
  if(auto* n = static_cast<count_in_node*>(m_node.get()))
    return n->beats.load(std::memory_order_relaxed);
  return 0;
}

ossia::net::parameter_base* Metronome::cueOutput(const score::DocumentContext& ctx)
{
  auto* devices = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devices)
    return nullptr;
  auto* audio = devices->list().audioDevice();
  if(!audio || !audio->getDevice())
    return nullptr;
  auto& root = audio->getDevice()->get_root_node();
  for(const char* path : {"/out/cue", "/out/main"})
    if(auto* node = ossia::net::find_node(root, path))
      if(auto* p = node->get_parameter())
        return p;
  return nullptr;
}

std::vector<int> Metronome::cueChannels(const score::DocumentContext& ctx)
{
  if(auto* p = dynamic_cast<ossia::mapped_audio_parameter*>(cueOutput(ctx)))
    return {p->mapping.begin(), p->mapping.end()};
  return {0, 1};
}

double Metronome::playedTempo(const Scenario::IntervalModel& played, TimeVal date)
{
  // Its own tempo, else the nearest box it is in that has one; then as fast as
  // the speeds of it and of the boxes it is in make it
  double tempo = ossia::root_tempo;
  for(const auto* itv = &played; itv; itv = Scenario::closestParentInterval(itv->parent()))
    if(itv->tempoCurve())
    {
      tempo = tempoAt(*itv, itv == &played ? date : TimeVal::zero());
      break;
    }
  for(const auto* itv = &played; itv; itv = Scenario::closestParentInterval(itv->parent()))
    tempo *= itv->duration.speed();
  return tempo;
}

ossia::time_signature
Metronome::playedSignature(const Scenario::IntervalModel& played, TimeVal date)
{
  for(const auto* itv = &played; itv; itv = Scenario::closestParentInterval(itv->parent()))
    if(itv->hasTimeSignature())
      return signatureAt(*itv, itv == &played ? date : TimeVal::zero());
  return {4, 4};
}

double Metronome::tempoAt(const Scenario::IntervalModel& root, TimeVal date)
{
  auto* tempo = root.tempoCurve();
  if(!tempo || tempo->duration().impl <= 0)
    return ossia::root_tempo;
  const double x = std::clamp(double(date.impl) / double(tempo->duration().impl), 0., 1.);
  const auto& segs = tempo->curve().sortedSegments();
  double y = 0.5;
  for(auto* seg : segs)
  {
    if(x >= seg->start().x() && x <= seg->end().x())
    {
      y = seg->valueAt(x);
      break;
    }
  }
  if(!segs.empty() && x > segs.back()->end().x())
    y = segs.back()->end().y();
  return y * (Scenario::TempoProcess::max - Scenario::TempoProcess::min)
         + Scenario::TempoProcess::min;
}

ossia::time_signature
Metronome::signatureAt(const Scenario::IntervalModel& root, TimeVal date)
{
  ossia::time_signature sig{4, 4};
  if(!root.hasTimeSignature())
    return sig;
  for(const auto& [t, s] : root.timeSignatureMap())
    if(t <= date)
      sig = s;
  return sig;
}

void Metronome::aboutToPlay(const Scenario::IntervalModel& root, TimeVal date)
{
  m_playing = &score::IDocument::documentContext(root);
  auto* hold = CountInHold::instance();
  if(!hold)
    return;
  hold->cancel();
  const auto& set = score::GUIAppContext().settings<Settings::Model>();
  if(!set.getCountInOnPlay())
    return;
  auto* exec = m_playing->findPlugin<Execution::DocumentPlugin>();
  if(!exec || !exec->context().execState)
    return;
  const int rate = int(exec->context().execState->sampleRate);
  if(rate <= 0)
    return;

  const double tempo = std::max(1., playedTempo(root, date));
  const auto sig = playedSignature(root, date);
  CountInHold::Count c;
  c.sounds = soundsOf(soundOf(set), rate);
  c.channels = cueChannels(*m_playing);
  c.volume = float(std::clamp(set.getClickVolume(), 0, 100)) / 100.f;
  c.accent = set.getClickAccent();
  c.beatsPerBar = std::max(1, int(sig.upper));
  c.beats = std::clamp(set.getCountInBars(), 1, 16) * c.beatsPerBar;
  // A beat is the signature's unit, the tempo counts quarter notes
  c.beatFrames = int64_t(std::llround(
      double(rate) * 60. / tempo * 4. / std::max(1., double(sig.lower))));
  hold->arm(std::move(c));
}

void Metronome::started()
{
  removeNode();
  if(!m_playing || !Execution::TapTicker::instance())
    return;
  auto* exec = m_playing->findPlugin<Execution::DocumentPlugin>();
  if(!exec || !exec->baseScenario())
    return;
  auto itv = exec->baseScenario()->baseInterval().OSSIAInterval();
  auto* param = cueOutput(*m_playing);
  const auto& ectx = exec->context();
  if(!itv || !param || !ectx.execState || !ectx.execGraph)
    return;

  const int rate = int(ectx.execState->sampleRate);
  const auto& set = score::GUIAppContext().settings<Settings::Model>();
  auto on = m_on;
  auto node = std::make_shared<count_in_node>(
      rate, itv->node, [on] { return on->load(std::memory_order_relaxed); });
  const int kind = soundOf(set);
  node->metro.sound = std::clamp(kind, 0, int(Media::Metro::Files) - 1);
  node->metro.volume = float(std::clamp(set.getClickVolume(), 0, 100)) / 100.f;
  node->metro.accent = set.getClickAccent();
  if(node->metro.sound == Media::Metro::Classic)
    if(auto s = Media::Metro::Model::classicSounds(rate))
      node->metro.swap_sounds(Media::Metro::Classic, s);

  m_node = node;
  ectx.executionQueue.enqueue([g = ectx.execGraph, ws = std::weak_ptr{ectx.execState},
                               node, param] {
    g->add_node(node);
    node->metro.audio_out.address = param;
    if(auto st = ws.lock())
      st->register_port(node->metro.audio_out);
    g->mark_dirty();
  });
  Execution::TapTicker::instance()->add(node);
}

void Metronome::stopped()
{
  if(auto* hold = CountInHold::instance())
    hold->cancel();
  removeNode();
}

void Metronome::removeNode()
{
  if(!m_node)
    return;
  if(auto* ticker = Execution::TapTicker::instance())
    ticker->remove(m_node.get());
  // The graph is torn down with the execution, the node with it
  m_node.reset();
}
}
