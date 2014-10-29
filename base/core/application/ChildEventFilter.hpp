#pragma once
#include <QObject>
#include <core/application/Application.hpp>
namespace iscore
{
	class ChildEventFilter : public QObject
	{
			Q_OBJECT
		public:
			ChildEventFilter(Application* app):
				m_app{app}
			{}

		protected:
			virtual bool eventFilter(QObject *obj, QEvent *ev) override
			{
				if(ev->type() == QEvent::ChildAdded)
				{
					m_app->doConnections(static_cast<QChildEvent*>(ev)->child());
					return true;
				}
				else
				{
					return QObject::eventFilter(obj, ev);
				}
			}

		private:
			Application* m_app;
	};
}
