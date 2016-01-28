#pragma once
#include <iscore/tools/Metadata.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <State/Expression.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT TriggerModel final : public IdentifiedObject<TriggerModel>
{
        Q_OBJECT

    public:
        TriggerModel(const Id<TriggerModel>& id, QObject* parent);

        State::Trigger expression() const;
        void setExpression(const State::Trigger& expression);
        bool isVoid();

        bool active() const;
        void setActive(bool active);

        // Note : this is for API -> UI communication.
        // To trigger by hand we have the triggered() signal.
        ExecutionStatusProperty executionStatus; // TODO serialize me ?

    signals:
        void triggerChanged(const State::Trigger&);
        void activeChanged();

        void triggered() const;

    private:
        State::Trigger m_expression;
        bool m_active {false};
};
}

DEFAULT_MODEL_METADATA(Scenario::TriggerModel, "Trigger")
