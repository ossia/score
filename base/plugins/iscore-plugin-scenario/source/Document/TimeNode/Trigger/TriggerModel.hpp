#pragma once
#include <QObject>

#include "State/Expression.hpp"

using namespace iscore;

class TriggerModel : public QObject
{
    Q_OBJECT

    public:
        TriggerModel(QObject* parent = 0);

        Trigger expression() const;
        void setExpression(const Trigger& expression);

    signals:
        void triggerChanged(Trigger);

    public slots:

    private:
        Trigger m_expression;
};
