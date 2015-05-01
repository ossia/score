#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/selection/Selection.hpp>
#include "State/ProcessStateDataInterface.hpp"

#include <ProcessInterface/ExpandMode.hpp>

class QDataStream;

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
        ProcessSharedModelInterface(TimeValue duration,
                                    id_type<ProcessSharedModelInterface> id,
                                    const QString& name,
                                    QObject* parent):
            IdentifiedObject<ProcessSharedModelInterface>{id, name, parent},
            m_duration{duration}
        {

        }

        virtual ProcessSharedModelInterface* clone(id_type<ProcessSharedModelInterface> newId,
                                                   QObject* newParent) = 0;

        /**
         * @brief processName
         * @return the name of the process.
         *
         * Needed for serialization - deserialization, in order to recreate
         * a new process from the same plug-in.
         */
        virtual QString processName() const = 0; // Needed for serialization.

        virtual ~ProcessSharedModelInterface() = default;

        //// View models interface
        // For deterministic operation in a command,
        // we have to generate some data (like ids...) before making a new view model.
        // This data is valid for construction only for the current state
        // of the scenario.
        virtual QByteArray makeViewModelConstructionData() const { return {}; }

        // TODO pass the name of the view model to be created
        // (e.g. temporal / logical...).
        virtual ProcessViewModelInterface* makeViewModel(id_type<ProcessViewModelInterface> viewModelId,
                                                         const QByteArray& constructionData,
                                                         QObject* parent) = 0;

        // To be called by createProcessViewModel only.
        virtual ProcessViewModelInterface* loadViewModel(const VisitorVariant&,
                                                         QObject* parent) = 0;

        // "Copy" factory. TODO replace by clone methode on PVM ?
        virtual ProcessViewModelInterface* cloneViewModel(id_type<ProcessViewModelInterface> newId,
                                                         const ProcessViewModelInterface* source,
                                                         QObject* parent) = 0;

        // Do a copy.
        QVector<ProcessViewModelInterface*> viewModels()
        {
            return m_viewModels;
        }

        //// Features of a process
        /// Duration
        // Used to scale the process.
        // This should be associative :
        //   setDurationWithScale(2); setDurationWithScale(3);
        // yields the same result as :
        //   setDurationWithScale(3); setDurationWithScale(2);
        virtual void setDurationAndScale(const TimeValue& newDuration) = 0;

        // Does nothing if newDuration < currentDuration
        virtual void setDurationAndGrow(const TimeValue& newDuration) = 0;

        // Does nothing if newDuration > currentDuration
        virtual void setDurationAndShrink(const TimeValue& newDuration) = 0;

        void expandProcess(ExpandMode mode, const TimeValue& t)
        {
            if(mode == ExpandMode::Scale)
            {
                setDurationAndScale(t);
            }
            else
            {
                if(duration() < t)
                    setDurationAndGrow(t);
                else
                    setDurationAndShrink(t);
            }
        }

        // TODO might not be useful... put in protected ?
        // Constructor needs it, too.
        void setDuration(const TimeValue& other)
        {
            m_duration = other;
        }

        TimeValue duration() const
        {
            return m_duration;
        }

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
        void addViewModel(ProcessViewModelInterface* m)
        {
            m_viewModels.push_back(m);
        }

        void removeViewModel(ProcessViewModelInterface* m)
        {
            int index = m_viewModels.indexOf(m);

            if(index != -1)
            {
                m_viewModels.remove(index);
            }
        }

    private:
        // Ownership ? The parent is the Deck. Always.
        // A process view is never displayed alone, it is always in a view, which is in a box.
        QVector<ProcessViewModelInterface*> m_viewModels;
        TimeValue m_duration;
};

template<typename T>
QVector<typename T::view_model_type*> viewModels(T* processModel)
{
    QVector<typename T::view_model_type*> v;

    for(auto& elt : processModel->viewModels())
    {
        v.push_back(static_cast<typename T::view_model_type*>(elt));
    }

    return v;
}

inline ProcessSharedModelInterface* parentProcess(QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !obj->inherits("ProcessSharedModelInterface"))
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
                QString("Object (name: %1) is not child of a Process!")
                .arg(objName)
                .toStdString());

    return static_cast<ProcessSharedModelInterface*>(obj);
}
inline const ProcessSharedModelInterface* parentProcess(const QObject* obj)
{
    QString objName (obj ? obj->objectName() : "INVALID");
    while(obj && !obj->inherits("ProcessSharedModelInterface"))
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
                QString("Object (name: %1) is not child of a Process!")
                .arg(objName)
                .toStdString());

    return static_cast<const ProcessSharedModelInterface*>(obj);
}
