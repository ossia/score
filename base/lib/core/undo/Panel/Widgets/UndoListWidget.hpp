#pragma once

#include <qlistwidget.h>

namespace iscore
{
class CommandStack;

class UndoListWidget final : public QListWidget
{
        Q_OBJECT
    public:
        explicit UndoListWidget(iscore::CommandStack* s);
        ~UndoListWidget();

    public slots:
        void on_stackChanged();

    private:
        iscore::CommandStack* m_stack{};
};

}
