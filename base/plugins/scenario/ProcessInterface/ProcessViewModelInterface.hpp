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
		ProcessViewModelInterface(QObject* parent, QString name, int viewModelId, ProcessSharedModelInterface* sharedProcess):
			IdentifiedObject{viewModelId, name, parent},
			m_sharedProcessModel{sharedProcess}
		{

		}

		virtual ~ProcessViewModelInterface() = default;

		ProcessSharedModelInterface* sharedProcessModel() const
		{ return m_sharedProcessModel; }

		virtual void serialize(QDataStream&) const = 0;
		virtual void deserialize(QDataStream&) = 0;

	private:
		ProcessSharedModelInterface* m_sharedProcessModel{};
};

inline QDataStream& operator <<(QDataStream& s, const ProcessViewModelInterface& p)
{
	s << static_cast<const IdentifiedObject&>(p)
	  << p.objectName();

	p.serialize(s);
	return s;
}

inline QDataStream& operator >>(QDataStream& s, ProcessViewModelInterface& p)
{
	QString name;
	s >> static_cast<IdentifiedObject&>(p)
	  >> name;

	p.setObjectName(name);

	p.deserialize(s);
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

