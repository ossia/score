#pragma once
#include <memory>
#include <QObject>

#include <core/presenter/command/CommandQueue.hpp>

namespace iscore
{
	class DocumentModel;
	class DocumentView;
	class DocumentPresenter : public QObject
	{
			Q_OBJECT
		public:
			DocumentPresenter(QObject* parent, DocumentModel*, DocumentView*);
			CommandQueue* commandQueue() { return m_commandQueue.get(); }

			void newDocument();
			void reset();

		public slots:

		private slots:
			void applyCommand(Command*);

		private:
			std::unique_ptr<CommandQueue> m_commandQueue;
	};
}
