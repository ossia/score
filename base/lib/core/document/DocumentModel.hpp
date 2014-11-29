#pragma once
#include <QObject>

namespace iscore
{
	class DocumentDelegateModelInterface;
	/**
	 * @brief The DocumentDelegateModelInterface class
	 *
	 * TODO
	 */
	class DocumentModel : public QObject
	{
		public:
			using QObject::QObject;
			void reset(){}
			void setModel(DocumentDelegateModelInterface* m);


		private:
			DocumentDelegateModelInterface* m_model{};
	};
}
