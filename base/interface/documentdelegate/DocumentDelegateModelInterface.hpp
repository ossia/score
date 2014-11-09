#pragma once
#include <QObject>

namespace iscore
{
	class DocumentDelegatePresenterInterface;
	class DocumentDelegateModelInterface : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			virtual ~DocumentDelegateModelInterface() = default;
			
			virtual void setPresenter(DocumentDelegatePresenterInterface* presenter) = 0;
	};
}
