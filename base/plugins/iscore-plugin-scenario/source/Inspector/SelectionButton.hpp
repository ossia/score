#pragma once
#include <QWidget>
#include <iscore/selection/Selection.hpp>
class QPushButton;
namespace iscore {
class SelectionDispatcher;
}
class SelectionButton : public QWidget
{
    public:
        SelectionButton(const QString& text,
                        Selection target,
                        iscore::SelectionDispatcher& disp,
                        QWidget* parent);

        template<typename Obj>
        static SelectionButton* make(Obj&& obj,
                              iscore::SelectionDispatcher& disp,
                              QWidget* parent)
        {
            return new SelectionButton{
                        QString::number(*obj->id().val()),
                        Selection{obj},
                        disp,
                        parent};
        }

    private:
        iscore::SelectionDispatcher& m_dispatcher;
        QPushButton* m_button{};
};
