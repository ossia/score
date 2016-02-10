#pragma once

#include <QGraphicsTextItem>

namespace Scenario
{

class TextItem final : public QGraphicsTextItem
{
    Q_OBJECT
    public:
    TextItem(QString text, QGraphicsObject* parent);

    signals:
    void focusOut();

    protected:
    void focusOutEvent(QFocusEvent* event) override;

};

}
