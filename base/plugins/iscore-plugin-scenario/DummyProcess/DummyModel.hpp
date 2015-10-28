#pragma once

#include <ProcessInterface/Process.hpp>

class DummyModel : public Process
{
    public:
        Process* clone(const Id<Process>& newId, QObject* newParent) const;
        QString processName() const;
        QString userFriendlyDescription() const;
        QByteArray makeLayerConstructionData() const;
        void setDurationAndScale(const TimeValue& newDuration);
        void setDurationAndGrow(const TimeValue& newDuration);
        void setDurationAndShrink(const TimeValue& newDuration);
        void startExecution();
        void stopExecution();
        void reset();
        ProcessStateDataInterface*startState() const;
        ProcessStateDataInterface*endState() const;
        Selection selectableChildren() const;
        Selection selectedChildren() const;
        void setSelection(const Selection& s) const;
        void serialize(const VisitorVariant& vis) const;

    protected:
        LayerModel* makeLayer_impl(const Id<LayerModel>& viewModelId, const QByteArray& constructionData, QObject* parent);
        LayerModel* loadLayer_impl(const VisitorVariant&, QObject* parent);
        LayerModel* cloneLayer_impl(const Id<LayerModel>& newId, const LayerModel& source, QObject* parent);
};
