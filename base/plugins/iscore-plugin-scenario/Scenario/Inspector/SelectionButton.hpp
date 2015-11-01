#pragma once
#include <QWidget>
#include <iscore/selection/Selection.hpp>
class QPushButton;
namespace iscore {
class SelectionDispatcher;
}
class SelectionButton final : public QWidget
{
    public:
        SelectionButton(const QString& text,
                        Selection target,
                        iscore::SelectionDispatcher& disp,
                        QWidget* parent);

        template<typename Obj>
        static SelectionButton* make(
                Obj&& obj,
                iscore::SelectionDispatcher& disp,
                QWidget* parent)
        {
            return new SelectionButton{
                QString::number(*obj->id().val()),
                Selection{obj},
                disp,
                parent};
        }

        template<typename Obj>
        static SelectionButton* make(
                const QString& text,
                Obj&& obj,
                iscore::SelectionDispatcher& disp,
                QWidget* parent)
        {
            auto but = new SelectionButton{
                text,
                Selection{obj},
                disp,
                parent};

            but->setToolTip(QString::number(*obj->id().val()));
            return but;
        }

    private:
        iscore::SelectionDispatcher& m_dispatcher;
        QPushButton* m_button{};
};
