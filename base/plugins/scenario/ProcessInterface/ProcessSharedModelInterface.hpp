#pragma once
#include <tools/IdentifiedObject.hpp>
class QDataStream;

class InspectorSectionWidget;
class ProcessViewModelInterface;
/**
	 * @brief The ProcessSharedModelInterface class
	 *
	 * Interface to implement to make a process.
	 */
class ProcessSharedModelInterface: public IdentifiedObject<ProcessSharedModelInterface>
{
	public:
		using IdentifiedObject<ProcessSharedModelInterface>::IdentifiedObject;

		/**
		 * @brief processName
		 * @return the name of the process.
		 *
		 * Needed for serialization - deserialization, in order to recreate
		 * a new process from the same plug-in.
		 */
		virtual QString processName() const = 0; // Needed for serialization.

		virtual ~ProcessSharedModelInterface() = default;

		//// View models interface
		// TODO pass the name of the view model to be created
		// (e.g. temporal / logical...).
		virtual ProcessViewModelInterface* makeViewModel(id_type<ProcessViewModelInterface> viewModelId,
														 QObject* parent) = 0;

		// To be called by createProcessViewModel only.
		virtual ProcessViewModelInterface* makeViewModel(SerializationIdentifier identifier,
														 void* data,
														 QObject* parent) = 0;

		// "Copy" factory
		virtual ProcessViewModelInterface* makeViewModel(id_type<ProcessViewModelInterface> newId,
														 const ProcessViewModelInterface* source,
														 QObject* parent) = 0;

		// Do a copy.
		QVector<ProcessViewModelInterface*> viewModels()
		{
			return m_viewModels;
		}

		// protected:
		virtual void serialize(SerializationIdentifier identifier,
							   void* data) const = 0;

	protected:
		void addViewModel(ProcessViewModelInterface* m)
		{
			m_viewModels.push_back(m);
		}

		void removeViewModel(ProcessViewModelInterface* m)
		{
			int index = m_viewModels.indexOf(m);

			if(index != -1)
				m_viewModels.remove(index);
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
