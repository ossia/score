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
class ISCORE_LIB_PROCESS_EXPORT LayerModel:
        public IdentifiedObject<LayerModel>
{
    public:
        virtual ~LayerModel();
        ProcessModel& processModel() const;

        virtual void serialize_impl(const VisitorVariant&) const = 0;


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

template<typename Process_T>
class LayerModel_T : public LayerModel
{
    public:
        explicit LayerModel_T(
                ProcessModel& model,
                const Id<Process::LayerModel>& id,
                QObject* parent):
            LayerModel{id, Metadata<ObjectKey_k, LayerModel_T>::get(), model, parent}
        {

        }

        explicit LayerModel_T(
                const LayerModel_T& other,
                ProcessModel& model,
                const Id<Process::LayerModel>& id,
                QObject* parent):
            LayerModel_T{model, id, parent}
        {

        }


        template<typename Impl>
        explicit LayerModel_T(
                Deserializer<Impl>& vis,
                ProcessModel& model,
                QObject* parent) :
            Process::LayerModel {vis, model, parent}
        {
            // Nothing to load
        }

        Process_T& processModel() const
        { return static_cast<Process_T&>(LayerModel::processModel()); }

    protected:
        // Nothing to save
        void serialize_impl(const VisitorVariant&) const override { }
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
