#pragma once
#include <Process/ProcessFactoryKey.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <QByteArray>
#include <QString>

class LayerModel;
class LayerPresenter;
class LayerView;
class Process;
class QGraphicsItem;
class QObject;
struct VisitorVariant;
template <typename tag, typename impl> class id_base_t;


/**
     * @brief The ProcessFactory class
     *
     * Interface to make processes, like Scenario, Automation...
     */

class ProcessFactory :
        public iscore::GenericFactoryInterface<ProcessFactoryKey>
{
        ISCORE_FACTORY_DECL("Process")
    public:
            using factory_key_type = ProcessFactoryKey;
        virtual ~ProcessFactory();
        virtual QString prettyName() const = 0;

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
