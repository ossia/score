#pragma once
#include <tools/NamedObject.hpp>

namespace iscore
{
	class DocumentDelegateModelInterface;
	class PanelModelInterface;
	/**
	 * @brief The DocumentDelegateModelInterface class
	 *
	 * Drawbridge between the application and a model given by a plugin.
	 */
	class DocumentModel : public NamedObject
	{
		public:
			DocumentModel(QObject* parent);
			void reset();
			void setModelDelegate(DocumentDelegateModelInterface* m);
			DocumentDelegateModelInterface* modelDelegate() const
			{ return m_model; }

			void addPanel(PanelModelInterface*);

		private:
			DocumentDelegateModelInterface* m_model{};
	};
}
