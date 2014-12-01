#pragma once
#include <QObject>
namespace iscore
{
	class DocumentDelegatePresenterInterface;
	class DocumentDelegateViewInterface : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			virtual ~DocumentDelegateViewInterface() = default;
			//virtual void setPresenter(DocumentDelegatePresenterInterface* presenter) = 0;

			// TODO what if QGraphicsObject / QML ?
			virtual QWidget* getWidget() = 0;
	};
}