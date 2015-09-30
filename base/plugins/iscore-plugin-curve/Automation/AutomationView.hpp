#pragma once

#include <ProcessInterface/LayerView.hpp>

class AutomationView : public LayerView
{
    public:
        explicit AutomationView(QGraphicsItem *parent);
        virtual void paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget);

        void setDisplayedName(QString s) {m_displayedName = s;}
        void showName(bool b) {m_showName = b;}

    private:
        QString m_displayedName{};
        bool m_showName{true};
};
