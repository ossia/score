#pragma once
#include <QStringList>
#include <QObject>
#include <QByteArray>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <ProcessInterface/TimeValue.hpp>
class LayerModel;
class ProcessModel;
class Layer;
class ProcessPresenter;

/**
     * @brief The ProcessFactory class
     *
     * Interface to make processes, like Scenario, Automation...
     */
class ProcessFactory : public iscore::FactoryInterface
{
    public:
        virtual ~ProcessFactory() = default;
        static QString factoryName();

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
                const LayerModel&,
                Layer*,
                QObject* parent) = 0;

        virtual Layer* makeView(
                const LayerModel& view,
                QObject* parent) = 0;



};
