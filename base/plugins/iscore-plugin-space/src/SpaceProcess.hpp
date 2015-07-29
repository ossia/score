#pragma once
#include <ProcessInterface/ProcessModel.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include "Area/AreaModel.hpp"
class SpaceModel;
class SpaceProcess : public Process
{
        Q_OBJECT
    public:
        SpaceProcess(const id_type<Process> &id, QObject *parent);
        Process *clone(const id_type<Process> &newId, QObject *newParent) const;

        QString processName() const;

        void setDurationAndScale(const TimeValue &newDuration);
        void setDurationAndGrow(const TimeValue &newDuration);
        void setDurationAndShrink(const TimeValue &newDuration);

        void reset();

        ProcessStateDataInterface *startState() const;
        ProcessStateDataInterface *endState() const;

        Selection selectableChildren() const;
        Selection selectedChildren() const;
        void setSelection(const Selection &s) const;

        void serialize(const VisitorVariant &vis) const;


        const auto& areas() const
        { return m_areas; }
        void addArea(AreaModel*);

    signals:
        void areaAdded(const AreaModel&);

    protected:
        LayerModel *makeLayer_impl(const id_type<LayerModel> &viewModelId, const QByteArray &constructionData, QObject *parent);
        LayerModel *loadLayer_impl(const VisitorVariant &, QObject *parent);
        LayerModel *cloneLayer_impl(const id_type<LayerModel> &newId, const LayerModel &source, QObject *parent);

    private:
        SpaceModel* m_space{};
        IdContainer<AreaModel> m_areas;
};
