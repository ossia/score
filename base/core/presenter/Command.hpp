#pragma once
#include <QUndoCommand>
#include <chrono>
namespace iscore
{
	// The base of the command system in i-score
	// It is timestamped, because we can then compare between clients
	class Command : public QUndoCommand
	{
		public:
			Command(const QString& text):
				QUndoCommand{text}
			{
			}

		protected:

		private:
			//TODO check if this is UTC
			std::chrono::milliseconds m_timestamp{std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())};
	};
}
