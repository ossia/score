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

			/*void setPresenter(DocumentDelegatePresenterInterface* presenter)
			{
				m_presenter = presenter;
			}*/

		//protected:
			//DocumentDelegatePresenterInterface* m_presenter;
	};
}
