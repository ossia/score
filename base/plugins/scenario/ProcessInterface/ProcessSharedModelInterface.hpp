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
		friend QDataStream& operator <<(QDataStream& s, const ProcessSharedModelInterface& proc)
		{
			s << proc.processName();
			s << static_cast<const IdentifiedObject&>(proc);

			proc.serializeImpl(s);
			return s;
		}

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
		virtual ProcessViewModelInterface* makeViewModel(int viewModelId,
														 QObject* parent) = 0;
		virtual ProcessViewModelInterface* makeViewModel(QDataStream& s,
														 QObject* parent) = 0;

		// Do a copy.
		QVector<QObject*> viewModels()
		{
			return m_viewModels;
		}

	protected:
		virtual void serializeImpl(QDataStream&) const = 0;
		virtual void deserializeImpl(QDataStream&) = 0;

		void addViewModel(QObject* m)
		{
			m_viewModels.push_back(m);
		}

		void removeViewModel(QObject* m)
		{
			m_viewModels.remove(m_viewModels.indexOf(m));
		}

	private:
		// Ownership ? The parent is the Deck. Always.
		// A process view is never displayed alone, it is always in a view, which is in a box.
		QVector<QObject*> m_viewModels;
};

