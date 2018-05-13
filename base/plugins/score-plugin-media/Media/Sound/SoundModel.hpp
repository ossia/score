#pragma once
#include <Media/MediaFileHandle.hpp>
#include <wobjectdefs.h>
#include <Media/Sound/SoundMetadata.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score_plugin_media_export.h>

namespace Media
{
namespace Sound
{
class ProcessModel;

class SCORE_PLUGIN_MEDIA_EXPORT ProcessModel final
    : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Sound::ProcessModel)

  W_OBJECT(ProcessModel)
  
  
public:
  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  ~ProcessModel() override;

  QString prettyName() const override;

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void setFile(const QString& file);
  void setFile(const MediaFileHandle& file);

  MediaFileHandle& file();
  const MediaFileHandle& file() const;

  int upmixChannels() const;
  int startChannel() const;

  void setUpmixChannels(int upmixChannels);
  void setStartChannel(int startChannel);

  std::unique_ptr<Process::Outlet> outlet;
public:
  void fileChanged() W_SIGNAL(fileChanged);
  void upmixChannelsChanged(int upmixChannels) W_SIGNAL(upmixChannelsChanged, upmixChannels);
  void startChannelChanged(int startChannel) W_SIGNAL(startChannelChanged, startChannel);

private:
  void init();

  MediaFileHandle m_file;
  int m_upmixChannels{};
  int m_startChannel{};

W_PROPERTY(int, startChannel READ startChannel WRITE setStartChannel NOTIFY startChannelChanged)

W_PROPERTY(int, upmixChannels READ upmixChannels WRITE setUpmixChannels NOTIFY upmixChannelsChanged)
};
}
}
