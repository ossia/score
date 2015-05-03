#pragma once
#include <QStringList>
#include <QObject>
#include <QByteArray>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <ProcessInterface/TimeValue.hpp>
class ProcessViewModel;
class ProcessModel;
class ProcessView;
class ProcessPresenter;

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

        virtual ProcessModel* makeModel(
                const TimeValue& duration,
                const id_type<ProcessModel>& id,
                QObject* parent) = 0;

        // throws if the serialization method is not implemented by the subclass
        virtual ProcessModel* loadModel(
                const VisitorVariant&,
                QObject* parent) = 0;

        // TODO Make it take a view name, too (cf. logical / temporal).
        // Or make it be created by the ViewModel, and the View be created by the presenter.
        virtual ProcessPresenter* makePresenter(
                const ProcessViewModel&,
                ProcessView*,
                QObject* parent) = 0;

        virtual ProcessView* makeView(
                const ProcessViewModel& view,
                QObject* parent) = 0;



};
