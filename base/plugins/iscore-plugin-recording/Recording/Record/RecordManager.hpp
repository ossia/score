#pragma once
#include <Recording/Record/RecordTools.hpp>
#include <Recording/Record/RecordData.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>

namespace boost
{
template<typename... U>
struct hash<eggs::variant<U...>>
{
        std::size_t operator()(const eggs::variant<U...>& k) const
        {
            return std::hash<eggs::variant<U...>>{}(k);
        }
};

template<>
struct hash<State::Address>
{
        std::size_t operator()(const State::Address& k) const
        {
            return std::hash<State::Address>{}(k);
        }
};
}
namespace std
{
  template <>
  struct hash<State::AddressAccessor>
  {
    std::size_t operator()(const State::AddressAccessor& k) const
    {
        std::size_t seed = 0;
        boost::hash_combine(seed, k.address);
        boost::hash_range(seed, k.qualifiers.accessors.begin(), k.qualifiers.accessors.end());
        boost::hash_combine(seed, k.qualifiers.unit);
        return seed;
    }
  };
}

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


        std::unordered_map<State::Address, RecordData> numeric_records;
        std::unordered_map<State::Address, std::array<RecordData, 2>> vec2_records;
        std::unordered_map<State::Address, std::array<RecordData, 3>> vec3_records;
        std::unordered_map<State::Address, std::array<RecordData, 4>> vec4_records;
        std::unordered_map<State::Address, std::vector<RecordData>> tuple_records;

    signals:
        void firstMessageReceived();

    private:
        void messageCallback(const State::Address& addr, const ossia::value& val);
        void parameterCallback(const State::Address& addr, const ossia::value& val);

        void finish(State::AddressAccessor addr, const RecordData& dat, const TimeValue& msecs, bool, int);
        const Curve::Settings::Model& m_settings;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

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
