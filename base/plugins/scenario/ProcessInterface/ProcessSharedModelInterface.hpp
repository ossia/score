#pragma once
#include <tools/IdentifiedObject.hpp>
class QDataStream;


class ProcessViewModelInterface;
/**
	 * @brief The ProcessSharedModelInterface class
	 *
	 * Interface to implement to make a process.
	 */
class ProcessSharedModelInterface: public IdentifiedObject
{


	public:
		using IdentifiedObject::IdentifiedObject;

		/**
			 * @brief processName
			 * @return the name of the process.
			 *
			 * Needed for serialization - deserialization, in order to recreate
			 * a new process from the same plug-in.
			 */
		virtual QString processName() const = 0; // Needed for serialization.

		virtual ~ProcessSharedModelInterface() = default;

		// TODO pass the name of the view model to be created
		// (e.g. temporal / logical...).
		virtual ProcessViewModelInterface* makeViewModel(int viewModelId,
														 QObject* parent) = 0;
		virtual ProcessViewModelInterface* makeViewModel(QDataStream& s,
														 QObject* parent) = 0;

		// Do a copy.
		QVector<ProcessViewModelInterface*> viewModels()
		{
			return m_viewModels;
		}

		// protected:
		/**
		 * @brief serialize
		 *
		 * We need to make a virtual function call in order to have the subclass serialize itself.
		 * An implementation of ProcessSharedModelInterface::serialize should only save
		 * data relative to the subclass.
		 *
		 * To save and load processes you have to use
		 * createProcess and saveProcess in ProcessViewModelInterfaceSerialization
		 */
		virtual void serialize(QDataStream&) const = 0;

		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const = 0;

	protected:
		void addViewModel(ProcessViewModelInterface* m)
		{
			m_viewModels.push_back(m);
		}

		void removeViewModel(ProcessViewModelInterface* m)
		{
			if(m_viewModels.contains(m))
				m_viewModels.remove(m_viewModels.indexOf(m));
		}

	private:
		// Ownership ? The parent is the Deck. Always.
		// A process view is never displayed alone, it is always in a view, which is in a box.
		QVector<ProcessViewModelInterface*> m_viewModels;
};

template<typename T>
QVector<typename T::view_model_type*> viewModels(T* processViewModel)
{
	QVector<typename T::view_model_type*> v;
	for(auto& elt : processViewModel->viewModels())
		v.push_back(static_cast<typename T::view_model_type*>(elt));
	return v;
}
