#pragma once
#include <QObject>
#include <core/application/Application.hpp>

namespace iscore
{
    /**
     * @brief The ChildEventFilter class filters the ChildAdded QEvent.
     *
     * This allow auto-connection to take place whenever a new object is created.
     *
     * NOTE: ChildEventFilter::eventFilter is called as soon as a QObject's parent is set.
     * Hence this will generally not work:
     * class A : public QObject
     * {
     *		A(QObject* parent): QObject{parent}
     *		{
     *			setObjectName("dumdum");
     *		}
     * }
     *
     * This is because the ChildAdded event is sent in QObject's constructor, and
     * the name is not set yet.
     * To solve this, please inherit from QNamedType<QObject,QWidget...>.
     * If this is not applicable, you can always re-call setParent later
     * and it will retrigger a ChildEvent.
     *
     */
    class ChildEventFilter : public QObject
    {
            Q_OBJECT
        public:
            ChildEventFilter (Application* app) :
                m_app {app}
            {}

        protected:
            virtual bool eventFilter (QObject* obj, QEvent* ev) override
            {
                if (ev->type() == QEvent::ChildAdded)
                {
                    m_app->doConnections (static_cast<QChildEvent*> (ev)->child() );
                    return true;
                }
                else
                {
                    return QObject::eventFilter (obj, ev);
                }
            }

        private:
            Application* m_app;
    };
}
