#include "ScenarioMetrics.hpp"
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <Automation/AutomationModel.hpp>

template<class>
class LanguageVisitor;

template<>
class LanguageVisitor<Scenario::ScenarioModel>
{
    public:
        const Scenario::ScenarioModel& m_scenar;
        QString text;

        LanguageVisitor(const Scenario::ScenarioModel& scenar):
            m_scenar{scenar}
        {
            text += "scenario " + id(scenar) + " {"
                  + QString("\n");

            for(const auto& elt : scenar.constraints)
            {
                visit(elt);
            }

            for(const auto& elt : scenar.events)
            {
                visit(elt);
            }

            for(const auto& elt : scenar.timeNodes)
            {
                visit(elt);
            }

            for(const auto& elt : scenar.states)
            {
                visit(elt);
            }

            text += "}"
                  + QString("\n");
        }

        template<typename T>
        QString id(const T& c)
        {
            return QString(T::description()) + QString::number(c.id_val());
        }

        QString duration(const ConstraintModel& c)
        {
            QString s;
            if(c.duration.isRigid())
            {
                s = QString::number(c.duration.defaultDuration().msec()) + "ms";
            }
            else
            {
                s += "[";
                s += QString::number(c.duration.minDuration().msec()) + "; ";
                if(c.duration.maxDuration().isInfinite())
                {
                    s += "oo";
                }
                else
                {
                    s += QString::number(c.duration.maxDuration().msec());
                }
                s += "]";
            }
            return s;
        }

        void visit(const ConstraintModel& c)
        {
            text += "constraint "   + id(c)
                  + " after state " + id(startState(c, m_scenar))
                  + " of duration " + duration(c)
                  + QString("\n");

            for(auto& process : c.processes)
            {
                if(auto scenar = dynamic_cast<Scenario::ScenarioModel*>(&process))
                {
                    text += "scenario " + id(*scenar)
                          + " inside " + id(c)
                          + QString("\n");

                    text += LanguageVisitor<Scenario::ScenarioModel>{*scenar}.text;
                }
            }
        }

        void visit(const EventModel& e)
        {
            text += "event " + id(e)
                  + " inside timenode " + id(parentTimeNode(e, m_scenar))
                  + QString("\n");


            if(e.condition().childCount() > 0)
            {
                text += "expression '" + e.condition().toString()
                      + "' inside event " + id(e)
                      + QString("\n");
            }
        }

        void visit(const TimeNodeModel& tn)
        {
            text += "timenode " + id(tn)
                  + QString("\n");

            if(tn.trigger() && tn.trigger()->expression().childCount() > 0)
            {
                text += "expression '" + tn.trigger()->expression().toString()
                      + "' inside timenode " + id(tn)
                      + QString("\n");
            }
        }

        void visit(const StateModel& st)
        {
            if(st.previousConstraint())
            {
                text += "state " + id(st)
                      + " after constraint " + id(previousConstraint(st, m_scenar))
                      + QString("\n");
            }

            text += "state " + id(st)
                  + " inside event " + id(parentEvent(st, m_scenar))
                  + QString("\n");
        }
};

QString Scenario::Metrics::toScenarioLanguage(const Scenario::ScenarioModel& s)
{
    return LanguageVisitor<Scenario::ScenarioModel>{s}.text;
}

int Scenario::Metrics::halstead(const Scenario::ScenarioModel& scenar)
{
    // How to count "default" values ?
    // Operateurs : association processus - contrainte et état - contrainte et état-evenement et état - timenode

    /// Quand. On créé une contrainte dans le vide : auto-completion d'etats de fin

    // Exposer structure d'arbre du scénario en js?
    // Cela permettrait de coder un visiteur super simplement pour calculer les valeurs de halstead

    // Faire parallèle entre data flow et time flow ? dans data flow, cable est un opérateur

    // Opérateurs:  after, inside, of duration, expression, (), constraint, state, event, timenode, scenario, loop, automation
    // ex.
    //
    // constraint c1 after s0 of duration 2s
    // state s2 after constraint c1
    // state s2 inside event e1
    // event e1 inside timenode t1
    // state s0 inside e0
    // event e0 inside t0
    // timenode t0
    // expression "(tze < 123)" inside event e0
    // scenario sx inside c1
    // scenario sx (
    //  ...
    // )
    // inside c1 {
    //   scenario
    //   csub0 after ssub0
    //}
    return 0;
}
