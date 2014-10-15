#pragma once
#include <QUndoCommand>
#include <chrono>
namespace iscore
{
	// The base of the command system in i-score
	// It is timestamped, because we can then compare between clients
	class Command : public QUndoCommand
	{
			using namespace std::chrono;
		Command(const QString& text):
			QUndoCommand{text}
		{
		}

		private:
			//TODO check if this is UTC
			std::chrono::milliseconds m_timestamp{duration_cast< milliseconds>(high_resolution_clock::now().time_since_epoch())};
	};
}
