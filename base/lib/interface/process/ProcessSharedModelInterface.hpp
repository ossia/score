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
															 int sharedProcessId,
															 QObject* parent) = 0;
			virtual ProcessViewModelInterface* makeViewModel(QDataStream& s,
															 QObject* parent) = 0;

	};
//}
