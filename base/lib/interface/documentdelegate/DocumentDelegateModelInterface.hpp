#pragma once
#include <tools/NamedObject.hpp>

namespace iscore
{
	class DocumentDelegatePresenterInterface;
	class DocumentDelegateModelInterface : public QNamedObject
	{
			Q_OBJECT
		public:
			using QNamedObject::QNamedObject;
			virtual ~DocumentDelegateModelInterface() = default;

			/*void setPresenter(DocumentDelegatePresenterInterface* presenter)
			{
				m_presenter = presenter;
			}*/

		//protected:
			//DocumentDelegatePresenterInterface* m_presenter;
	};
}
