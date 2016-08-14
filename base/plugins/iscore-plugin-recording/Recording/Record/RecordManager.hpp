#pragma once
#include <Recording/Record/RecordTools.hpp>
#include <Recording/Record/RecordData.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>

namespace Curve
{
namespace Settings
{
class Model;
}
}
namespace Recording
{
struct RecordContext;
// TODO for some reason we have to undo redo
// to be able to send the curve at execution. Investigate why.
class AutomationRecorder :
        public QObject,
        public RecordProvider
{
        Q_OBJECT
    public:
        RecordContext& context;
        AutomationRecorder(RecordContext& ctx);

        bool setup(const Box&, const RecordListening&) override;
        void stop() override;

        void commit();

    signals:
        void firstMessageReceived();

    private:
        void messageCallback(const State::Address& addr, const State::Value& val);
        void parameterCallback(const State::Address& addr, const State::Value& val);

        const Curve::Settings::Model& m_settings;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;


        std::unordered_map<
            State::Address,
            RecordData> records;
        // TODO see this : http://stackoverflow.com/questions/34596768/stdunordered-mapfind-using-a-type-different-than-the-key-type
};

class AutomationRecorderFactory final :
        public RecorderFactory
{
        Priority matches(
            const Device::Node&,
            const iscore::DocumentContext& ctx) override;

        std::unique_ptr<RecordProvider> make(
            const Device::NodeList&,
            const iscore::DocumentContext& ctx) override;

};

}
