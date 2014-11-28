#pragma once
#include <QGLWidget>

namespace iscore
{
	/**
	 * @brief The DocumentView class is the central view of i-score.
	 *
	 * It displays the @c{PanelView} of a @c{BasePanel}.
	 */
	// TODO prévoir une vue tabbée ou on puisse voir plusieurs documents ?
	// Comment gérer les liens d'un plug-in à un document ? (ex. réseau ?)
	class DocumentView : public QGLWidget
	{
		public:
			DocumentView(QWidget* parent);
	};
}
