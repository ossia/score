#pragma once
#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/SoundMetadata.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <ossia/dataflow/audio_stretch_mode.hpp>

#include <verdigris>

Q_DECLARE_METATYPE(ossia::audio_stretch_mode)
W_REGISTER_ARGTYPE(ossia::audio_stretch_mode)
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

  int upmixChannels() const noexcept;
  int startChannel() const noexcept;
  double nativeTempo() const noexcept;
  ossia::audio_stretch_mode stretchMode() const noexcept;

  void setUpmixChannels(int upmixChannels);
  void setStartChannel(int startChannel);
  void setNativeTempo(double);
  void setStretchMode(ossia::audio_stretch_mode);

  void on_mediaChanged();

  std::unique_ptr<Process::AudioOutlet> outlet;

public:
  void fileChanged()
      W_SIGNAL(fileChanged);
  void nativeTempoChanged(double t)
      W_SIGNAL(nativeTempoChanged, t);
  void upmixChannelsChanged(int upmixChannels)
      W_SIGNAL(upmixChannelsChanged, upmixChannels);
  void startChannelChanged(int startChannel)
      W_SIGNAL(startChannelChanged, startChannel);
  void stretchModeChanged(ossia::audio_stretch_mode mode)
      W_SIGNAL(stretchModeChanged, mode);

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
      double,
      nativeTempo READ nativeTempo WRITE setNativeTempo NOTIFY
      nativeTempoChanged,
      W_Final)
  PROPERTY(
      ossia::audio_stretch_mode,
      stretchMode READ stretchMode WRITE setStretchMode NOTIFY
      stretchModeChanged,
      W_Final)

private:
  void init();

  std::shared_ptr<AudioFile> m_file;
  int m_upmixChannels{};
  int m_startChannel{};
  ossia::audio_stretch_mode m_mode{};
  double m_nativeTempo{};
};
}

optional<double> estimateTempo(const AudioFile& file);
}
