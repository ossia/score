#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

class ProcessSharedModelInterface;
class ProcessViewModelPanelProxy;

/**
 * @brief The ProcessViewModelInterface class
 *
 * Interface to implement to make a process view model.
 */
class ProcessViewModelInterface: public IdentifiedObject<ProcessViewModelInterface>
{
    public:
        virtual ~ProcessViewModelInterface() = default;

        ProcessSharedModelInterface* sharedProcessModel() const
        {
            return m_sharedProcessModel;
        }

        // protected:
        virtual void serialize(const VisitorVariant&) const = 0;

        virtual ProcessViewModelPanelProxy* make_panelProxy() = 0;


    protected:
        ProcessViewModelInterface(id_type<ProcessViewModelInterface> viewModelId,
                                  QString name,
                                  ProcessSharedModelInterface* sharedProcess,
                                  QObject* parent) :
            IdentifiedObject<ProcessViewModelInterface> {viewModelId, name, parent},
        m_sharedProcessModel {sharedProcess}
        {

        }

        template<typename Impl>
        ProcessViewModelInterface(Deserializer<Impl>& vis,
                                  ProcessSharedModelInterface* sharedProcess,
                                  QObject* parent) :
            IdentifiedObject<ProcessViewModelInterface> {vis, parent},
            m_sharedProcessModel {sharedProcess}
        {
            // Nothing else to load
        }

    private:
        ProcessSharedModelInterface* m_sharedProcessModel {};
};


/**
 * @brief model Returns the casted version of a shared model given a view model.
 * @param viewModel A view model which has a directive "using model_type = MySharedModelType;" in its class body
 *
 * @return a pointer of the correct type.
 */
template<typename T>
typename T::model_type* model(T* viewModel)
{
    return static_cast<typename T::model_type*>(viewModel->sharedProcessModel());
}


template<typename T>
typename T::model_type& model(T& viewModel)
{
    return static_cast<typename T::model_type&>(*viewModel.sharedProcessModel());
}


/**
 * @brief identifierOfViewModelFromSharedModel
 * @param pvm A process view model
 *
 * @return A tuple which contains the required identifiers to get from a shared model to a given view model
 *  * The box identifier
 *  * The deck identifier
 *  * The view model identifier
 */
inline std::tuple<int, int, int> identifierOfViewModelFromSharedModel(ProcessViewModelInterface* pvm)
{
    // TODO - this only works in a scenario.
    auto deckModel = static_cast<IdentifiedObjectAbstract*>(pvm->parent());
    auto boxModel = static_cast<IdentifiedObjectAbstract*>(deckModel->parent());
    return std::tuple<int, int, int>
    {
        boxModel->id_val(),
        deckModel->id_val(),
        pvm->id_val()
    };
}

inline QDataStream& operator<< (QDataStream& s, const std::tuple<int, int, int>& tuple)
{
    s << std::get<0> (tuple) << std::get<1> (tuple) << std::get<2> (tuple);
    return s;
}
inline QDataStream& operator>> (QDataStream& s, std::tuple<int, int, int>& tuple)
{
    s >> std::get<0> (tuple) >> std::get<1> (tuple) >> std::get<2> (tuple);
    return s;
}
