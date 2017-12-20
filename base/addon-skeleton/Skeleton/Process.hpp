#pragma once
#include <Process/Process.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Skeleton/Metadata.hpp>

namespace Skeleton
{
class Model final
    : public Process::ProcessModel
{
    SCORE_SERIALIZE_FRIENDS
    PROCESS_METADATA_IMPL(Skeleton::Model)
    Q_OBJECT

    public:
      Model(
        const TimeVal& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent);

    template<typename Impl>
    Model(Impl& vis, QObject* parent) :
      Process::ProcessModel{vis, parent}
    {
      vis.writeTo(*this);
    }

    ~Model();

  private:

    QString prettyName() const override;
    void startExecution() override;
    void stopExecution() override;
    void reset() override;

    void setDurationAndScale(const TimeVal& newDuration) override;
    void setDurationAndGrow(const TimeVal& newDuration) override;
    void setDurationAndShrink(const TimeVal& newDuration) override;
};

using ProcessFactory = Process::GenericProcessModelFactory<Skeleton::Model>;
}
