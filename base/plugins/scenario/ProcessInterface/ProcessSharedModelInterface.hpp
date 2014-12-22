#pragma once
#include <tools/IdentifiedObject.hpp>
class QDataStream;
//namespace iscore
//{
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

		protected:
			virtual void serializeImpl(QDataStream&) const = 0;
			virtual void deserializeImpl(QDataStream&) = 0;

	};
//}
