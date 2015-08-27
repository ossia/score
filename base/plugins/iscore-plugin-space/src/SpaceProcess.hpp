#pragma once
#include <ProcessInterface/ProcessModel.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include "Area/AreaModel.hpp"
#include "Computation/ComputationModel.hpp"
class SpaceModel;
class SpaceProcess : public Process
{
        Q_OBJECT
    public:
        SpaceProcess(const Id<Process> &id, QObject *parent);
        Process *clone(const Id<Process> &newId, QObject *newParent) const;

        QString processName() const;

        void setDurationAndScale(const TimeValue &newDuration);
        void setDurationAndGrow(const TimeValue &newDuration);
        void setDurationAndShrink(const TimeValue &newDuration);

        void reset();

        ProcessStateDataInterface *startState() const;
        ProcessStateDataInterface* endState() const;

        Selection selectableChildren() const;
        Selection selectedChildren() const;
        void setSelection(const Selection &s) const;

        void serialize(const VisitorVariant &vis) const;

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
        LayerModel *makeLayer_impl(const Id<LayerModel> &viewModelId, const QByteArray &constructionData, QObject *parent);
        LayerModel *loadLayer_impl(const VisitorVariant &, QObject *parent);
        LayerModel *cloneLayer_impl(const Id<LayerModel> &newId, const LayerModel &source, QObject *parent);

    private:
        SpaceModel* m_space{};
        IdContainer<AreaModel> m_areas;
        IdContainer<ComputationModel> m_computations;

        // Default viewport
};
