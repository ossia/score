#pragma once
#include <QWidget>

namespace iscore
{
	class DocumentDelegateViewInterface;

	/**
	 * @brief The DocumentView class is the central view of i-score.
	 *
	 * It displays a @c{DocumentDelegateViewInterface}.
	 */
	class DocumentView : public QWidget
	{
		public:
			DocumentView(QWidget* parent);
			void setViewDelegate(DocumentDelegateViewInterface*);

			void reset();

		private:
			DocumentDelegateViewInterface* m_view{};
	};
}
