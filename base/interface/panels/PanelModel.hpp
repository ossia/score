#pragma once
#include <QObject>

namespace iscore
{
	class PanelPresenter;
	class PanelModel : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			virtual ~PanelModel() = default;
			
			virtual void setPresenter(PanelPresenter* presenter) = 0;
	};
}
