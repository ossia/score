#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include "State/ProcessStateDataInterface.hpp"

#include <ProcessInterface/ExpandMode.hpp>

class LayerModel;
/**
 * @brief The ProcessModel class
 *
 * Interface to implement to make a process.
 */
class ProcessModel: public IdentifiedObject<ProcessModel>
{
        Q_OBJECT
    public:
        using IdentifiedObject<ProcessModel>::IdentifiedObject;
        ProcessModel(
                const TimeValue& duration,
                const id_type<ProcessModel>& id,
                const QString& name,
                QObject* parent);

        virtual ProcessModel* clone(
                const id_type<ProcessModel>& newId,
                QObject* newParent) = 0;

        virtual QString processName() const = 0; // Needed for serialization.

        //// View models interface
        // For deterministic operation in a command,
        // we have to generate some data (like ids...) before making a new view model.
        // This data is valid for construction only for the current state
        // of the scenario.
        virtual QByteArray makeViewModelConstructionData() const;

        // TODO pass the name of the view model to be created
        // (e.g. temporal / logical...).
        LayerModel* makeViewModel(
                const id_type<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent);

        // Load
        LayerModel* loadViewModel(
                const VisitorVariant& v,
                QObject* parent);

        // Clone
        LayerModel* cloneViewModel(
                const id_type<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent);

        // For use where the view model is ephemeral (e.g. process panel)
        LayerModel* makeTemporaryViewModel(
                const id_type<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent);
        // Do a copy.
        QVector<LayerModel*> viewModels() const;

        //// Features of a process
        /// Duration
        // Used to scale the process.
        // This should be commutative :
        //   setDurationWithScale(2); setDurationWithScale(3);
        // yields the same result as :
        //   setDurationWithScale(3); setDurationWithScale(2);
        virtual void setDurationAndScale(const TimeValue& newDuration) = 0;

        // Does nothing if newDuration < currentDuration
        virtual void setDurationAndGrow(const TimeValue& newDuration) = 0;

        // Does nothing if newDuration > currentDuration
        virtual void setDurationAndShrink(const TimeValue& newDuration) = 0;

        void expandProcess(ExpandMode mode, const TimeValue& t);

        // TODO might not be useful... put in protected ?
        // Constructor needs it, too.
        void setDuration(const TimeValue& other);

        const TimeValue& duration() const;

        /// States
        virtual ProcessStateDataInterface* startState() const = 0;
        virtual ProcessStateDataInterface* endState() const = 0;

        /// Selection
        virtual Selection selectableChildren() const = 0;
        virtual Selection selectedChildren() const = 0;
        virtual void setSelection(const Selection& s) const = 0;

        // protected:
        virtual void serialize(const VisitorVariant& vis) const = 0;

    protected:
        virtual LayerModel* makeViewModel_impl(
                const id_type<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) = 0;
        virtual LayerModel* loadViewModel_impl(
                const VisitorVariant&,
                QObject* parent) = 0;
        virtual LayerModel* cloneViewModel_impl(
                const id_type<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent) = 0;


    private:
        void addViewModel(LayerModel* m);
        void removeViewModel(LayerModel* m);

        // Ownership : the parent is the Slot or another widget, not the process.
        // A process view is never displayed alone, it is always in a view, which is in a box.
        QVector<LayerModel*> m_viewModels;
        TimeValue m_duration;
};

template<typename T>
QVector<typename T::view_model_type*> viewModels(const T& processModel)
{
    QVector<typename T::view_model_type*> v;

    for(auto& elt : processModel.viewModels())
    {
        v.push_back(static_cast<typename T::view_model_type*>(elt));
    }

    return v;
}

ProcessModel* parentProcess(QObject* obj);
const ProcessModel* parentProcess(const QObject* obj);
