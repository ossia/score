#pragma once
#include <tools/IdentifiedObject.hpp>

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

		virtual void serialize(QDataStream&) const = 0;

	protected:
		ProcessViewModelInterface(int viewModelId,
								  QString name,
								  ProcessSharedModelInterface* sharedProcess,
								  QObject* parent):
			IdentifiedObject{viewModelId, name, parent},
			m_sharedProcessModel{sharedProcess}
		{

		}

		ProcessViewModelInterface(QDataStream& s,
								  ProcessSharedModelInterface* sharedProcess,
								  QObject* parent):
			IdentifiedObject{s, parent},
			m_sharedProcessModel{sharedProcess}
		{
			// In derived classes's constructors, do s >> *this; (there is nothing to do here)
		}

	private:
		ProcessSharedModelInterface* m_sharedProcessModel{};
};

inline QDataStream& operator <<(QDataStream& s,
								const ProcessViewModelInterface& p)
{
	s << static_cast<const IdentifiedObject&>(p);

	p.serialize(s);
	return s;
}


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
				boxModel->id(),
				deckModel->id(),
				pvm->id()};
}

