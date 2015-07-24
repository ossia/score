#pragma once
#include <ProcessInterface/ProcessModel.hpp>

class SpaceProcess : public Process
{
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

    protected:
        LayerModel *makeLayer_impl(const id_type<LayerModel> &viewModelId, const QByteArray &constructionData, QObject *parent);
        LayerModel *loadLayer_impl(const VisitorVariant &, QObject *parent);
        LayerModel *cloneLayer_impl(const id_type<LayerModel> &newId, const LayerModel &source, QObject *parent);
};
