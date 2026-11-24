#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/Process.hpp>

#include <Media/Sound/SoundModel.hpp>

#include <score/model/Component.hpp>

#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/value/value.hpp>

namespace Media
{
class SoundComponentSetup;
class AudioFile;
}
namespace ossia
{
class sound_node;
}
namespace Execution
{

class SoundComponent final
    : public ::Execution::ProcessComponent_T<
          Media::Sound::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("a25d0de0-74e2-4011-aeb6-4188673015f2")
public:
  SoundComponent(
      Media::Sound::ProcessModel& element, const ::Execution::Context& ctx,
      QObject* parent);

  void recompute();
  void on_fileChanged();
  //! What plays: the file a port asked for while playing, else the document's
  std::shared_ptr<Media::AudioFile> currentFile() const noexcept;
  //! A node hands the paths its file inlet gets to playFile
  void wire(ossia::sound_node& n);
  void playFile(const QString& path);

  ~SoundComponent() override;

private:
  friend class Media::SoundComponentSetup;
  struct Recomputer : public Nano::Observer
  {
    explicit Recomputer(SoundComponent& self)
        : self{self}
    {
    }
    SoundComponent& self;
    void recompute() { self.recompute(); }
    void live() { self.liveFileReady(); }
  };
  void liveFileReady();
  Recomputer m_recomputer;
  Recomputer m_live{*this};
  std::shared_ptr<Media::AudioFile> m_liveFile;
};

using SoundComponentFactory = ::Execution::ProcessComponentFactory_T<SoundComponent>;

SCORE_CONCRETE_COMPONENT_FACTORY(
    Execution::ProcessComponentFactory, Execution::SoundComponentFactory)
}
