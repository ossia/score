#pragma once
#include <Process/Process.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <Media/Sound/SoundMetadata.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Process/Dataflow/Port.hpp>
#include <score_plugin_media_export.h>


namespace Media
{
namespace Sound
{
class ProcessModel;

class SCORE_PLUGIN_MEDIA_EXPORT ProcessModel final : public Process::ProcessModel
{
        SCORE_SERIALIZE_FRIENDS
        PROCESS_METADATA_IMPL(Media::Sound::ProcessModel)

        Q_OBJECT
        Q_PROPERTY(int upmixChannels READ upmixChannels WRITE setUpmixChannels NOTIFY upmixChannelsChanged)
        Q_PROPERTY(int startChannel READ startChannel WRITE setStartChannel NOTIFY startChannelChanged)
    public:
        Process::Inlets inlets() const override;
        Process::Outlets outlets() const override;

        explicit ProcessModel(
                const TimeVal& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent);


        ~ProcessModel() override;

        template<typename Impl>
        explicit ProcessModel(
                Impl& vis,
                QObject* parent) :
            Process::ProcessModel{vis, parent}
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
signals:
        void fileChanged();
        void upmixChannelsChanged(int upmixChannels);
        void startChannelChanged(int startChannel);

private:
        void init();

        MediaFileHandle m_file;
        int m_upmixChannels{};
        int m_startChannel{};
};

}
}
