#pragma once
#include <QStringList>
#include <QObject>
#include <QByteArray>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <Process/TimeValue.hpp>
class LayerModel;
class Process;
class LayerView;
class LayerPresenter;
class QGraphicsItem;

/**
     * @brief The ProcessFactory class
     *
     * Interface to make processes, like Scenario, Automation...
     */
class ProcessFactory : public iscore::FactoryInterface
{
    public:
        virtual ~ProcessFactory();
        static QString factoryName();

        // The process name
        virtual QString name() const = 0;

        virtual Process* makeModel(
                const TimeValue& duration,
                const Id<Process>& id,
                QObject* parent) = 0;

        // The layers may need some specific static data to construct,
        // this provides it (for the sake of commands)
        virtual QByteArray makeStaticLayerConstructionData() const = 0;

        // throws if the serialization method is not implemented by the subclass
        virtual Process* loadModel(
                const VisitorVariant&,
                QObject* parent) = 0;

        // TODO Make it take a view name, too (cf. logical / temporal).
        // Or make it be created by the ViewModel, and the View be created by the presenter.
        virtual LayerPresenter* makeLayerPresenter(
                const LayerModel&,
                LayerView*,
                QObject* parent) = 0;

        virtual LayerView* makeLayerView(
                const LayerModel& view,
                QGraphicsItem* parent) = 0;

};
