#pragma once
#include <QWidget>

namespace iscore
{
	/**
	 * @brief The DocumentView class is the central view of i-score.
	 *
	 * It displays a @c{DocumentDelegateViewInterface}.
	 */
	class DocumentView : public QWidget
	{
		public:
			DocumentView(QWidget* parent);
	};
}
