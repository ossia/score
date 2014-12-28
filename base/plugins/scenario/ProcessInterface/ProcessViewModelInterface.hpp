#pragma once
#include <tools/IdentifiedObject.hpp>
#include "serialization/VisitorInterface.hpp"

class ProcessSharedModelInterface;

/**
 * @brief The ProcessViewModelInterface class
 *
 * Interface to implement to make a process view model.
 */
class ProcessViewModelInterface: public IdentifiedObject
{
	public:
		virtual ~ProcessViewModelInterface() = default;

		ProcessSharedModelInterface* sharedProcessModel() const
		{ return m_sharedProcessModel; }

		// protected:
		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const = 0;

	protected:

		ProcessViewModelInterface(int viewModelId,
								  QString name,
								  ProcessSharedModelInterface* sharedProcess,
								  QObject* parent):
			IdentifiedObject{viewModelId, name, parent},
			m_sharedProcessModel{sharedProcess}
		{

		}

		template<typename Impl>
		ProcessViewModelInterface(Deserializer<Impl>& vis,
								  ProcessSharedModelInterface* sharedProcess,
								  QObject* parent):
			IdentifiedObject{vis, parent},
			m_sharedProcessModel{sharedProcess}
		{
			// Nothing else to load
		}

	private:
		ProcessSharedModelInterface* m_sharedProcessModel{};
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
	auto deckModel = static_cast<IdentifiedObject*>(pvm->parent());
	auto boxModel = static_cast<IdentifiedObject*>(deckModel->parent());
	return std::tuple<int,int,int>{
				(SettableIdentifier::identifier_type) boxModel->id(),
				(SettableIdentifier::identifier_type) deckModel->id(),
				(SettableIdentifier::identifier_type) pvm->id()};
}

