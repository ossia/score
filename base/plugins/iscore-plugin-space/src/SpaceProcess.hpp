#pragma once
#include <Process/Process.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include "Area/AreaModel.hpp"
#include "Computation/ComputationModel.hpp"
class SpaceModel;
class SpaceProcess : public Process
{
        Q_OBJECT
    public:
        SpaceProcess(const TimeValue &duration, const Id<Process> &id, QObject *parent);
        Process *clone(const Id<Process> &newId, QObject *newParent) const override;

        QString processName() const override;
        QString userFriendlyDescription() const override
        { return tr("Space process"); }

        void setDurationAndScale(const TimeValue &newDuration) override;
        void setDurationAndGrow(const TimeValue &newDuration) override;
        void setDurationAndShrink(const TimeValue &newDuration) override;

        void reset() override;

        ProcessStateDataInterface *startState() const override;
        ProcessStateDataInterface* endState() const override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection &s) const override;

        void serialize(const VisitorVariant &vis) const override;

        const SpaceModel& space() const
        { return *m_space; }

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
        LayerModel *makeLayer_impl(
                const Id<LayerModel> &viewModelId,
                const QByteArray &constructionData,
                QObject *parent) override;
        LayerModel *loadLayer_impl(
                const VisitorVariant &,
                QObject *parent) override;
        LayerModel *cloneLayer_impl(
                const Id<LayerModel> &newId,
                const LayerModel &source,
                QObject *parent) override;

    private:
        SpaceModel* m_space{};
        IdContainer<AreaModel> m_areas;
        IdContainer<ComputationModel> m_computations;

        // Default viewport

        // Process interface
    public:
        void startExecution() override;
        void stopExecution() override;
};
