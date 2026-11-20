#include "MetroExecutor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <Media/MediaFileHandle.hpp>
#include <Media/Metro/MetroNode.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QPointer>

namespace Execution
{
namespace
{
using Media::Metro::metronome_node;

class metronome_process final : public ossia::node_process
{
public:
  using ossia::node_process::node_process;

  // The node does not run while the process is stopped or muted: a click cut
  // there would otherwise end when it runs again.
  void start() override
  {
    silence();
    ossia::node_process::start();
  }

  void stop() override
  {
    silence();
    ossia::node_process::stop();
  }

  void mute_impl(bool b) override
  {
    silence();
    ossia::node_process::mute_impl(b);
  }

private:
  void silence() noexcept { static_cast<metronome_node*>(node.get())->reset(); }
};

constexpr Media::Metro::sound_kind file_kinds[]
    = {Media::Metro::Classic, Media::Metro::Files};
}

MetroComponent::MetroComponent(
    Media::Metro::Model& element, const Execution::Context& ctx, QObject* parent)
    : Execution::ProcessComponent_T<Media::Metro::Model, ossia::node_process>{
        element, ctx, "Executor::MetroComponent", parent}
{
  const int rate = ctx.execState->sampleRate;
  auto node = ossia::make_node<metronome_node>(*ctx.execState, rate);
  node->sound = ossia::convert<int>(element.sound->value());
  node->volume = ossia::convert<float>(element.volume->value());
  node->accent = ossia::convert<bool>(element.accent->value());
  node->subdivision = ossia::convert<int>(element.subdivision->value());
  node->beat = ossia::convert<int>(element.beat->value());

  node->set_file_path(0, ossia::convert<std::string>(element.accentFile->value()));
  node->set_file_path(1, ossia::convert<std::string>(element.beatFile->value()));

  // The audio thread only passes the path on: it is decoded from this thread,
  // and several paths in a row are one decoding, of the last.
  node->on_file = [queue = weak_edit, self = QPointer{this}](
                      int file, const std::string& path) {
    if(auto q = queue.lock())
      q->enqueue([self, file, path = std::string{path}]() mutable {
        if(self)
          self->on_executionPath(file, QString::fromStdString(path));
      });
  };

  // Not running yet: what is already decoded is set without the queue.
  for(auto kind : file_kinds)
  {
    auto set = element.sounds(kind, rate);
    node->swap_sounds(kind, set);
  }

  this->node = node;
  m_ossia_process = std::make_shared<metronome_process>(node);

  element.sound->setupExecution(node->sound_in, this);
  element.accentFile->setupExecution(node->accent_file_in, this);
  element.beatFile->setupExecution(node->beat_file_in, this);
  element.volume->setupExecution(node->volume_in, this);
  element.accent->setupExecution(node->accent_in, this);
  element.subdivision->setupExecution(node->subdivision_in, this);
  element.beat->setupExecution(node->beat_in, this);

  // The interface writes the ports' values; cables and automations reach the
  // node's inlets directly.
  con(*element.sound, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, c = ossia::convert<int>(v)] { node->sound = c; });
  });
  con(*element.volume, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, c = ossia::convert<float>(v)] { node->volume = c; });
  });
  con(*element.accent, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, c = ossia::convert<bool>(v)] { node->accent = c; });
  });
  con(*element.subdivision, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, c = ossia::convert<int>(v)] { node->subdivision = c; });
  });
  con(*element.beat, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, c = ossia::convert<int>(v)] { node->beat = c; });
  });

  con(*element.accentFile, &Process::ControlInlet::valueChanged, this,
      [this](const ossia::value& v) { on_modelPath(0, v); });
  con(*element.beatFile, &Process::ControlInlet::valueChanged, this,
      [this](const ossia::value& v) { on_modelPath(1, v); });

  con(element, &Media::Metro::Model::soundsChanged, this, &MetroComponent::updateSounds);
}

void MetroComponent::on_executionPath(int file, QString path)
{
  if(file < 0 || file >= 2)
    return;
  m_files[file].pending = std::move(path);

  // Once the queue is empty: the paths behind this one in it replace it.
  if(!m_scheduled)
  {
    m_scheduled = true;
    QMetaObject::invokeMethod(this, [this] { decodePending(); }, Qt::QueuedConnection);
  }
}

void MetroComponent::decodePending()
{
  m_scheduled = false;
  bool changed = false;
  for(auto& f : m_files)
  {
    if(!f.pending)
      continue;
    if(f.file)
      f.file->on_finishedDecoding.disconnect<&Decoded::changed>(m_decoded);

    // Null for no path, a file that is not there or not a click: this inlet
    // then has no sound, as an empty control.
    f.file = Media::Metro::Model::decodeFile(*f.pending, process());
    f.pending.reset();
    f.active = true;
    if(f.file)
    {
      m_decodes++;
      f.file->on_finishedDecoding.connect<&Decoded::changed>(m_decoded);
    }
    changed = true;
  }

  // What played until now is replaced at once; the new sound follows when it
  // is decoded.
  if(changed)
    updateSounds();
}

void MetroComponent::on_modelPath(int file, const ossia::value& path)
{
  auto& f = m_files[file];
  if(f.file)
    f.file->on_finishedDecoding.disconnect<&Decoded::changed>(m_decoded);
  f = {};

  if(auto node = std::dynamic_pointer_cast<metronome_node>(this->node))
    in_exec([node, file, p = ossia::convert<std::string>(path)] {
      node->set_file_path(file, p);
    });
  updateSounds();
}

void MetroComponent::updateSounds()
{
  auto node = std::dynamic_pointer_cast<metronome_node>(this->node);
  if(!node)
    return;

  const int rate = system().execState->sampleRate;
  const auto& accent = m_files[0].active ? m_files[0].file : process().accentSound();
  const auto& beat = m_files[1].active ? m_files[1].file : process().beatSound();

  // The sounds the node had come back in the command, and are freed with it
  // outside of the audio thread.
  using namespace Media::Metro;
  in_exec([node, set = process().sounds(Classic, rate)]() mutable {
    node->swap_sounds(Classic, set);
  });
  in_exec([node, set = Model::fileSounds(accent, beat, rate)]() mutable {
    node->swap_sounds(Files, set);
  });
}

MetroComponent::~MetroComponent() { }
}
