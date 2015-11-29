#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <qstring.h>

#include "iscore/serialization/VisitorInterface.hpp"

class LayerModelPanelProxy;
class Process;
class QObject;
template <typename tag, typename impl> class id_base_t;

/**
 * @brief The LayerModel class
 *
 * Interface to implement to make a process view model.
 */
class LayerModel: public IdentifiedObject<LayerModel>
{
        ISCORE_METADATA("LayerModel")
    public:
        virtual ~LayerModel();
        Process& processModel() const;

        virtual void serialize(const VisitorVariant&) const = 0;
        virtual LayerModelPanelProxy* make_panelProxy(QObject* parent) const = 0;


    protected:
        // TODO this argument order sucks
        LayerModel(const Id<LayerModel>& viewModelId,
                   const QString& name,
                   Process& sharedProcess,
                   QObject* parent);

        template<typename Impl>
        LayerModel(Deserializer<Impl>& vis,
                   Process& sharedProcess,
                   QObject* parent) :
            IdentifiedObject{vis, parent},
            m_sharedProcessModel {sharedProcess}
        {
            // Nothing else to load
        }

    private:
        Process& m_sharedProcessModel;
};


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
