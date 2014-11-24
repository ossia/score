#pragma once
#include <QWidget>

namespace iscore
{
	class PanelPresenterInterface;
	class PanelViewInterface : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			virtual ~PanelViewInterface() = default;
			virtual void setPresenter(PanelPresenterInterface* presenter) = 0;
			virtual QWidget* getWidget() = 0; 
	};
}
