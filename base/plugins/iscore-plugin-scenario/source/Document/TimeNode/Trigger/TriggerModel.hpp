#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include "State/Expression.hpp"

using namespace iscore;

class TriggerModel : public IdentifiedObject<TriggerModel>
{
    Q_OBJECT

    public:
        TriggerModel(const Id<TriggerModel>& id, QObject* parent = 0);

        iscore::Trigger expression() const;
        void setExpression(const iscore::Trigger& expression);
        bool isVoid();

        bool active() const;
        void setActive(bool active);

    signals:
        void triggerChanged(const iscore::Trigger&);
        void activeChanged();

    public slots:

    private:
        iscore::Trigger m_expression;
        bool m_active {false};
};
