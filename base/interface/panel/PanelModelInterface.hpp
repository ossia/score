#pragma once
#include <QObject>

namespace iscore
{
	class PanelPresenterInterface;
	class PanelModelInterface : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			virtual ~PanelModelInterface() = default;
			
			virtual void setPresenter(PanelPresenterInterface* presenter) = 0;
	};
}
