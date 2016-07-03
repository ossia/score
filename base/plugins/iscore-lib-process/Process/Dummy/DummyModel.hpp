#pragma once
#include <Process/Dummy/DummyState.hpp>
#include <Process/Process.hpp>
#include <QByteArray>
#include <QString>


#include <Process/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore_lib_process_export.h>


class DataStream;
class JSONObject;
namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Dummy
{
class ISCORE_LIB_PROCESS_EXPORT DummyModel final :
        public Process::ProcessModel
{
        ISCORE_SERIALIZE_FRIENDS(DummyModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(DummyModel, JSONObject)

    public:
        explicit DummyModel(
                const TimeValue& duration,
                const Id<ProcessModel>& id,
                QObject* parent);

        explicit DummyModel(
                const DummyModel& source,
                const Id<ProcessModel>& id,
                QObject* parent);

        template<typename Impl>
        explicit DummyModel(
                Deserializer<Impl>& vis,
                QObject* parent) :
            ProcessModel{vis, parent}
        {
            vis.writeTo(*this);
        }

    private:
        DummyModel* clone(
                const Id<ProcessModel>& newId,
                QObject* newParent) const override;

        UuidKey<Process::ProcessFactory>concreteFactoryKey() const override;
        QString prettyName() const override;

        ProcessStateDataInterface* startStateData() const override;
        ProcessStateDataInterface* endStateData() const override;

        void serialize_impl(const VisitorVariant& vis) const override;

        mutable DummyState m_startState{*this, nullptr};
        mutable DummyState m_endState{*this, nullptr};
};
}
