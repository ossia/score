#pragma once
#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/SoundMetadata.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <wobjectdefs.h>
namespace Media
{
namespace Sound
{
class ProcessModel;

class ProcessModel final : public Process::ProcessModel, public Nano::Observer
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Sound::ProcessModel)

  W_OBJECT(ProcessModel)

public:
  explicit ProcessModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id,
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
  qint32 startOffset() const;

  void setUpmixChannels(int upmixChannels);
  void setStartChannel(int startChannel);
  void setStartOffset(qint32 startOffset);

  void on_mediaChanged();

  std::unique_ptr<Process::Outlet> outlet;

public:
  void fileChanged() W_SIGNAL(fileChanged);
  void upmixChannelsChanged(int upmixChannels)
      W_SIGNAL(upmixChannelsChanged, upmixChannels);
  void startChannelChanged(int startChannel)
      W_SIGNAL(startChannelChanged, startChannel);
  void startOffsetChanged(qint32 startOffset)
      W_SIGNAL(startOffsetChanged, startOffset);

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
  PROPERTY(
      int,
      startOffset READ startOffset WRITE setStartOffset NOTIFY
          startOffsetChanged,
      W_Final)

private:
  void init();

  MediaFileHandle m_file;
  int m_upmixChannels{};
  int m_startChannel{};
  qint32 m_startOffset{};
  qint32 m_endOffset{};
};
}
}
