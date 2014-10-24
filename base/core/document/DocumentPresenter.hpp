#pragma once
#include <memory>
#include <QObject>

#include <core/presenter/command/CommandQueue.hpp>

#include <API/Headers/Repartition/session/Session.h>
#include <API/Headers/Repartition/session/ConnectionData.hpp>

namespace iscore
{
	class DocumentModel;
	class DocumentView;
	class DocumentPresenter : public QObject
	{
			Q_OBJECT
		public:
			DocumentPresenter(DocumentModel*, DocumentView*);
			Session* session() { return m_networkSession.get(); }
			CommandQueue* commandQueue() { return m_commandQueue.get(); }

		public slots:
			void setupClientSession(ConnectionData d);

		private slots:
			void setupMasterSession();
			void applyCommand(Command*);

		private:
			std::unique_ptr<Session> m_networkSession; // For distribution
			std::unique_ptr<CommandQueue> m_commandQueue;
	};
}
