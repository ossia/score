#pragma once
#include <QTextEdit>

class CommentEdit final : public QTextEdit
{
    Q_OBJECT
    public:
        template<typename... Args>
        CommentEdit(Args&&... args):
            QTextEdit{std::forward<Args>(args)...}
        {
            setMouseTracking(true);
        }

        void leaveEvent(QEvent* ev) override
        {
            QTextEdit::leaveEvent(ev);
            emit editingFinished();
        }

    signals:
        void editingFinished();
};
