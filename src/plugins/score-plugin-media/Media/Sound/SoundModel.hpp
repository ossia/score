#pragma once
#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/SoundMetadata.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <verdigris>
namespace Media
{
namespace Sound
{
class ProcessModel;

class SCORE_PLUGIN_MEDIA_EXPORT ProcessModel final
    : public Process::ProcessModel,
      public Nano::Observer
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Sound::ProcessModel)

  W_OBJECT(ProcessModel)

public:
  explicit ProcessModel(
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  ~ProcessModel() override;

  QString prettyName() const noexcept override;

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
      , m_file{std::make_shared<AudioFile>()}
  {
    vis.writeTo(*this);
    init();
  }

  void setFile(const QString& file);
  void setFile(const AudioFile& file);

  std::shared_ptr<AudioFile>& file();
  const std::shared_ptr<AudioFile>& file() const;

  int upmixChannels() const;
  int startChannel() const;

  void setUpmixChannels(int upmixChannels);
  void setStartChannel(int startChannel);

  void on_mediaChanged();

  std::unique_ptr<Process::Outlet> outlet;

public:
  void fileChanged() W_SIGNAL(fileChanged);
  void upmixChannelsChanged(int upmixChannels)
      W_SIGNAL(upmixChannelsChanged, upmixChannels);
  void startChannelChanged(int startChannel)
      W_SIGNAL(startChannelChanged, startChannel);

  PROPERTY(
      int,
      startChannel READ startChannel WRITE setStartChannel NOTIFY
          startChannelChanged,
      W_Final)
  PROPERTY(
      int,
      upmixChannels READ upmixChannels WRITE setUpmixChannels NOTIFY
          upmixChannelsChanged,
      W_Final)

private:
  void init();

  std::shared_ptr<AudioFile> m_file;
  int m_upmixChannels{};
  int m_startChannel{};
};
}
}
