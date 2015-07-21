#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

class ProcessModel;
class LayerModelPanelProxy;

/**
 * @brief The LayerModel class
 *
 * Interface to implement to make a process view model.
 */
class LayerModel: public IdentifiedObject<LayerModel>
{
    public:
        ProcessModel& sharedProcessModel() const;

        virtual void serialize(const VisitorVariant&) const = 0;
        virtual LayerModelPanelProxy* make_panelProxy(QObject* parent) const = 0;


    protected:
        LayerModel(const id_type<LayerModel>& viewModelId,
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


/**
 * @brief model Returns the casted version of a shared model given a view model.
 * @param viewModel A view model which has a directive "using model_type = MySharedModelType;" in its class body
 *
 * @return a pointer of the correct type.
 */
template<typename T>
const typename T::model_type* model(const T* viewModel)
{
    return static_cast<const typename T::model_type*>(viewModel->sharedProcessModel());
}


template<typename T>
const typename T::model_type& model(const T& viewModel)
{
    return static_cast<const typename T::model_type&>(viewModel.sharedProcessModel());
}
