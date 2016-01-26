#pragma once
#include <iscore/tools/Metadata.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore_lib_process_export.h>
#include <QString>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class QObject;
namespace Process
{
class LayerModelPanelProxy;
class ProcessModel;

/**
 * @brief The LayerModel class
 *
 * Interface to implement to make a process view model.
 */
class ISCORE_LIB_PROCESS_EXPORT LayerModel: public IdentifiedObject<LayerModel>
{
    public:
        virtual ~LayerModel();
        ProcessModel& processModel() const;

        virtual void serialize(const VisitorVariant&) const = 0;
        virtual LayerModelPanelProxy* make_panelProxy(QObject* parent) const = 0;


    protected:
        // TODO this argument order sucks
        LayerModel(const Id<LayerModel>& viewModelId,
                   const QString& name,
                   ProcessModel& sharedProcess,
                   QObject* parent);

        template<typename Impl>
        LayerModel(Deserializer<Impl>& vis,
                   ProcessModel& sharedProcess,
                   QObject* parent) :
            IdentifiedObject{vis, parent},
            m_sharedProcessModel {sharedProcess}
        {
            // Nothing else to load
        }

    private:
        ProcessModel& m_sharedProcessModel;
};
}

/**
 * @brief model Returns the casted version of a shared model given a view model.
 * @param viewModel A view model which has a directive "using model_type = MySharedModelType;" in its class body
 *
 * @return a pointer of the correct type.
 */
template<typename T>
typename T::model_type* model(const T* viewModel)
{
    return static_cast<typename T::model_type*>(viewModel->processModel());
}


template<typename T>
typename T::model_type& model(const T& viewModel)
{
    return static_cast<typename T::model_type&>(viewModel.processModel());
}

DEFAULT_MODEL_METADATA(Process::LayerModel, "LayerModel")
