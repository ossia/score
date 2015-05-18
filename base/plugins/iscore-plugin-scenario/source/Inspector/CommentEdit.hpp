#pragma once
#include <QTextEdit>

class CommentEdit : public QTextEdit
{
    Q_OBJECT
    public:
        template<typename... Args>
        CommentEdit(Args&&... args):
            QTextEdit{std::forward<Args&&>(args)...}
        {
            setMouseTracking(true);
        }

        virtual void leaveEvent(QEvent* ev) override
        {
            QTextEdit::leaveEvent(ev);
            emit editingFinished();
        }

    signals:
        void editingFinished();
};
