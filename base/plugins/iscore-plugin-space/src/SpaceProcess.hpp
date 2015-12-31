#pragma once
#include <Process/Process.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include "Area/AreaModel.hpp"
#include "Computation/ComputationModel.hpp"
#include <OSSIA/DocumentPlugin/ProcessModel/ProcessModel.hpp>
#include <iscore/tools/NotifyingMap.hpp>

#include <OSSIA/LocalTree/Scenario/ProcessComponent.hpp>
#include <OSSIA/LocalTree/Scenario/MetadataParameters.hpp>
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

class ProcessExecutor final : public TimeProcessWithConstraint
{
    public:
        ProcessExecutor(Space::ProcessModel& process);


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
        Space::ProcessModel& m_process;

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
                const Id<Process::ProcessModel> &id,
                QObject *parent);
        ProcessModel *clone(const Id<Process::ProcessModel> &newId, QObject *newParent) const override;

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

        NotifyingMap<AreaModel> areas;
        NotifyingMap<ComputationModel> computations;

    protected:
        Process::LayerModel *makeLayer_impl(
                const Id<Process::LayerModel> &viewModelId,
                const QByteArray &constructionData,
                QObject *parent) override;
        Process::LayerModel *loadLayer_impl(
                const VisitorVariant &,
                QObject *parent) override;
        Process::LayerModel *cloneLayer_impl(
                const Id<Process::LayerModel> &newId,
                const Process::LayerModel &source,
                QObject *parent) override;

    private:
        void startExecution() override;
        void stopExecution() override;
        std::shared_ptr<TimeProcessWithConstraint> process() const override;

        SpaceModel* m_space{};
        Space::AreaContext m_context;
        std::shared_ptr<Space::ProcessExecutor> m_process;

};



class ProcessLocalTree final :
        public OSSIA::LocalTree::ProcessComponent
{
        COMPONENT_METADATA(SpaceProcessLocalTree)

         using system_t = OSSIA::LocalTree::DocumentPlugin;

     public:
        ProcessLocalTree(
                const Id<Component>& id,
                OSSIA::Node& parent,
                Space::ProcessModel& process,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_obj);

        std::shared_ptr<OSSIA::Node> m_areas;
        std::shared_ptr<OSSIA::Node> m_computations;
        std::vector<std::unique_ptr<BaseProperty>> m_properties;

};


class ProcessLocalTreeFactory final :
        public OSSIA::LocalTree::ProcessComponentFactory
{
    public:
        virtual ~ProcessLocalTreeFactory();
        const factory_key_type& key_impl() const override;

        bool matches(
                Process::ProcessModel& p,
                const OSSIA::LocalTree::DocumentPlugin&,
                const iscore::DocumentContext&) const override;

        OSSIA::LocalTree::ProcessComponent* make(
                const Id<iscore::Component>& id,
                OSSIA::Node& parent,
                Process::ProcessModel& proc,
                const OSSIA::LocalTree::DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const override;
};
}
