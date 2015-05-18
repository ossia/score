#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

class ProcessModel;
class ProcessViewModelPanelProxy;

/**
 * @brief The ProcessViewModel class
 *
 * Interface to implement to make a process view model.
 */
class ProcessViewModel: public IdentifiedObject<ProcessViewModel>
{
    public:
        ProcessModel& sharedProcessModel() const;

        virtual void serialize(const VisitorVariant&) const = 0;
        virtual ProcessViewModelPanelProxy* make_panelProxy(QObject* parent) const = 0;


    protected:
        ProcessViewModel(const id_type<ProcessViewModel>& viewModelId,
                         const QString& name,
                         ProcessModel& sharedProcess,
                         QObject* parent);

        template<typename Impl>
        ProcessViewModel(Deserializer<Impl>& vis,
                         ProcessModel& sharedProcess,
                         QObject* parent) :
            IdentifiedObject<ProcessViewModel> {vis, parent},
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
