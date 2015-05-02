#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include "State/ProcessStateDataInterface.hpp"

#include <ProcessInterface/ExpandMode.hpp>

class ProcessViewModelInterface;
/**
 * @brief The ProcessSharedModelInterface class
 *
 * Interface to implement to make a process.
 */
class ProcessSharedModelInterface: public IdentifiedObject<ProcessSharedModelInterface>
{
        Q_OBJECT
    public:
        using IdentifiedObject<ProcessSharedModelInterface>::IdentifiedObject;
        ProcessSharedModelInterface(
                const TimeValue& duration,
                const id_type<ProcessSharedModelInterface>& id,
                const QString& name,
                QObject* parent);

        virtual ProcessSharedModelInterface* clone(
                const id_type<ProcessSharedModelInterface>& newId,
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
        ProcessViewModelInterface* makeViewModel(
                const id_type<ProcessViewModelInterface>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent);

        // Load
        ProcessViewModelInterface* loadViewModel(
                const VisitorVariant& v,
                QObject* parent);

        // Clone
        ProcessViewModelInterface* cloneViewModel(
                const id_type<ProcessViewModelInterface>& newId,
                const ProcessViewModelInterface& source,
                QObject* parent);

        // Do a copy.
        QVector<ProcessViewModelInterface*> viewModels() const;

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
        virtual ProcessViewModelInterface* makeViewModel_impl(
                const id_type<ProcessViewModelInterface>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) = 0;
        virtual ProcessViewModelInterface* loadViewModel_impl(
                const VisitorVariant&,
                QObject* parent) = 0;
        virtual ProcessViewModelInterface* cloneViewModel_impl(
                const id_type<ProcessViewModelInterface>& newId,
                const ProcessViewModelInterface& source,
                QObject* parent) = 0;


    private:
        void addViewModel(ProcessViewModelInterface* m);
        void removeViewModel(ProcessViewModelInterface* m);

        // Ownership : the parent is the Deck or another widget, not the process.
        // A process view is never displayed alone, it is always in a view, which is in a box.
        QVector<ProcessViewModelInterface*> m_viewModels;
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

ProcessSharedModelInterface* parentProcess(QObject* obj);
const ProcessSharedModelInterface* parentProcess(const QObject* obj);
