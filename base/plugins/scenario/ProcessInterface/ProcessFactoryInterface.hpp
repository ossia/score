#pragma once
#include <QStringList>
#include <QObject>
#include <QByteArray>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class ProcessViewModelInterface;
class ProcessSharedModelInterface;
class ProcessViewInterface;
class ProcessPresenterInterface;

/**
     * @brief The ProcessFactoryInterface class
     *
     * Interface to make processes, like Scenario, Automation...
     */
class ProcessFactoryInterface : public iscore::FactoryInterface
{
    public:
        virtual ~ProcessFactoryInterface() = default;

        // The process name
        virtual QString name() const = 0;

        virtual ProcessViewInterface* makeView(ProcessViewModelInterface* view, QObject* parent) = 0;

        // TODO Make it take a view name, too (cf. logical / temporal).
        // Or make it be created by the ViewModel, and the View be created by the presenter.
        virtual ProcessPresenterInterface* makePresenter(ProcessViewModelInterface*,
                ProcessViewInterface*,
                QObject* parent) = 0;


        virtual ProcessSharedModelInterface* makeModel(id_type<ProcessSharedModelInterface> id, QObject* parent)  = 0;
        // virtual ProcessSharedModelInterface* makeModel(QDataStream& data, QObject* parent)  = 0;

        // throws if the serialization method is not implemented by the subclass
        virtual ProcessSharedModelInterface* makeModel(SerializationIdentifier identifier,
                void* data, // Todo : use a variant of some kind instead?
                QObject* parent)  = 0;
};
