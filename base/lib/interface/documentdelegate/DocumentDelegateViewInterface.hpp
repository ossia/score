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

			// TODO what if QGraphicsObject / QML ?
			virtual QWidget* getWidget() = 0;
	};
}