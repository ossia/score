#pragma once
#include <QObject>

namespace iscore
{
	class DocumentPanelPresenter;
	class DocumentPanelModel : public QObject
	{
			Q_OBJECT
		public:
			using QObject::QObject;
			virtual ~DocumentPanelModel() = default;
			
			virtual void setPresenter(DocumentPanelPresenter* presenter) = 0;
	};
}
