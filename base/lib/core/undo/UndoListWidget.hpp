#pragma once
#include <QListWidget>

namespace iscore
{
    class CommandStack;
    class UndoListWidget : public QListWidget
    {
            Q_OBJECT
        public:
            UndoListWidget(iscore::CommandStack* s);
            ~UndoListWidget();

        public slots:
            void on_stackChanged();

        private:
            iscore::CommandStack* m_stack{};
    };

}
