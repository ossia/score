#pragma once
#include <QObject>
namespace iscore
{
	class DocumentPanelPresenter;
	class DocumentPanelView : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			virtual ~DocumentPanelView() = default;
			virtual void setPresenter(DocumentPanelPresenter* presenter) = 0;
			
			// TODO what if QGraphicsObject / QML ?
			virtual QWidget* getWidget() = 0; 
	};
}