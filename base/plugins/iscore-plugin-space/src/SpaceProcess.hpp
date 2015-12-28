#pragma once
#include <Process/Process.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include "Area/AreaModel.hpp"
#include "Computation/ComputationModel.hpp"
#include <OSSIA/DocumentPlugin/ProcessModel/ProcessModel.hpp>
class SpaceModel;

namespace Space
{
class LayerModel;
class ProcessModel;
struct ProcessMetadata
{
        static const ProcessFactoryKey& factoryKey()
        {
            static const ProcessFactoryKey name{"Space"};
            return name;
        }

        static QString processObjectName()
        {
            return "Space";
        }

        static QString factoryPrettyName()
        {
            return QObject::tr("Space");
        }
};

class Process final : public TimeProcessWithConstraint
{
    public:
        Process(ProcessModel& process);


        std::shared_ptr<OSSIA::StateElement> state(
                const OSSIA::TimeValue&,
                const OSSIA::TimeValue&) override;

        const std::shared_ptr<OSSIA::State>& getStartState() const override
        {
            return m_start;
        }

        const std::shared_ptr<OSSIA::State>& getEndState() const override
        {
            return m_end;
        }


    private:
        ProcessModel& m_process;

        std::shared_ptr<OSSIA::State> m_start;
        std::shared_ptr<OSSIA::State> m_end;
};



class ProcessModel : public RecreateOnPlay::OSSIAProcessModel
{
        Q_OBJECT
    public:
        ProcessModel(
                const iscore::DocumentContext& doc,
                const TimeValue &duration,
                const Id<::Process> &id,
                QObject *parent);
        ProcessModel *clone(const Id<::Process> &newId, QObject *newParent) const override;

        const ProcessFactoryKey& key() const override;
        QString prettyName() const override;

        void setDurationAndScale(const TimeValue &newDuration) override;
        void setDurationAndGrow(const TimeValue &newDuration) override;
        void setDurationAndShrink(const TimeValue &newDuration) override;

        void reset() override;

        ProcessStateDataInterface *startStateData() const override;
        ProcessStateDataInterface* endStateData() const override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection &s) const override;

        void serialize(const VisitorVariant &vis) const override;

        const SpaceModel& space() const
        { return *m_space; }
        const Space::AreaContext& context() const
        { return m_context; }

        const auto& areas() const
        { return m_areas; }
        void addArea(AreaModel*);
        void removeArea(const Id<AreaModel>& id);

        const auto& computations() const
        { return m_computations; }
        void addComputation(ComputationModel*);

    signals:
        void areaAdded(const AreaModel&);
        void areaRemoved(const Id<AreaModel>&);
        void computationAdded(const ComputationModel&);

    protected:
        ::LayerModel *makeLayer_impl(
                const Id<::LayerModel> &viewModelId,
                const QByteArray &constructionData,
                QObject *parent) override;
        ::LayerModel *loadLayer_impl(
                const VisitorVariant &,
                QObject *parent) override;
        ::LayerModel *cloneLayer_impl(
                const Id<::LayerModel> &newId,
                const ::LayerModel &source,
                QObject *parent) override;

    private:
        void startExecution() override;
        void stopExecution() override;
        std::shared_ptr<TimeProcessWithConstraint> process() const override;

        SpaceModel* m_space{};
        IdContainer<AreaModel> m_areas;
        IdContainer<ComputationModel> m_computations;
        Space::AreaContext m_context;
        std::shared_ptr<Space::Process> m_process;

};
}
