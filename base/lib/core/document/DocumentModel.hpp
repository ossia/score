#pragma once
#include <QObject>

namespace iscore
{
	class DocumentDelegateModelInterface;
	/**
	 * @brief The DocumentDelegateModelInterface class
	 *
	 * Drawbridge between the application and a model given by a plugin.
	 */
	class DocumentModel : public QObject
	{
		public:
			using QObject::QObject;
			void reset();
			void setModelDelegate(DocumentDelegateModelInterface* m);

		private:
			DocumentDelegateModelInterface* m_model{};
	};
}
