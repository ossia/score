#include "ScenarioMetrics.hpp"
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <iscore/tools/std/Algorithms.hpp>

#include <Automation/AutomationModel.hpp>

template<class>
class LanguageVisitor;

/*
    // How to count "default" values ?
    // Operateurs : association processus - contrainte et état - contrainte et état-evenement et état - timenode

    /// Quand. On créé une contrainte dans le vide : auto-completion d'etats de fin

    // Exposer structure d'arbre du scénario en js?
    // Cela permettrait de coder un visiteur super simplement pour calculer les valeurs de halstead

    // Faire parallèle entre data flow et time flow ? dans data flow, cable est un opérateur

    // Opérateurs:  after, of, duration, expression, {}, [;], constraint, state, event, timenode, scenario, loop, automation
    // ex.
    //
    // constraint c1 after s0
    // duration [2, oo] of c1

    // scenario sx {
    // ...
    // } of c1

    // state s2 after c1
    // state s2 of e1
    // event e1 of t1
    // state s0 of e0
    // event e0 of t0
    // timenode t0
    // expression {(tze < 123)} of e0
    // scenario sx of c1
    */
template<>
class LanguageVisitor<Scenario::ScenarioModel>
{
    public:
        const Scenario::ScenarioModel& m_scenar;
        QString text;

        LanguageVisitor(const Scenario::ScenarioModel& scenar):
            m_scenar{scenar}
        {
            text += " {"
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

            text += "}";
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
                  + " after " + id(startState(c, m_scenar))
                  + QString("\n");
            text += "duration " + duration(c)
                  + " of " + id(c)
                  + QString("\n");

            for(auto& process : c.processes)
            {
                if(auto scenar = dynamic_cast<Scenario::ScenarioModel*>(&process))
                {
                    text += "scenario " + id(*scenar)
                          + LanguageVisitor<Scenario::ScenarioModel>{*scenar}.text
                          + " of " + id(c)
                          + QString("\n");
                }
            }
        }

        void visit(const EventModel& e)
        {
            text += "event " + id(e)
                  + " of " + id(parentTimeNode(e, m_scenar))
                  + QString("\n");


            if(e.condition().childCount() > 0)
            {
                text += "expression '" + e.condition().toString()
                      + "' of " + id(e)
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
                      + "' of " + id(tn)
                      + QString("\n");
            }
        }

        void visit(const StateModel& st)
        {
            if(st.previousConstraint())
            {
                text += "state " + id(st)
                      + " after " + id(previousConstraint(st, m_scenar))
                      + QString("\n");
            }

            text += "state " + id(st)
                  + " of " + id(parentEvent(st, m_scenar))
                  + QString("\n");
        }
};

struct ScenarioFactors
{
    // Operators
        struct operators_t {
                int scenario{};

                int constraint{};
                int event{};
                int state{};
                int timenode{};

                int of{};
                int after{};
                int lbrace{};
                int rbrace{};

                int expression{};
                int duration{};

                std::vector<int> toVector() const
                {
                    return {scenario, constraint, event, state, timenode, of, after, lbrace, rbrace, expression, duration};
                }

        } operators;

    // Operands
        struct operands_t {
                QMap<QString, int> variables;
                std::vector<QMap<QString, int>> subprocesses_variables;
                int expressions{};
                int constraint_rigid_times{};
                int constraint_minmax_times{};


                std::vector<int> toVector() const
                {
                    std::vector<int> v;

                    for(auto elt : variables)
                    {
                        v.push_back(elt);
                    }

                    for(auto vec : subprocesses_variables)
                    {
                        for(auto elt : vec)
                        {
                            v.push_back(elt);
                        }
                    }

                    v.push_back(expressions);
                    v.push_back(constraint_rigid_times);
                    v.push_back(constraint_minmax_times);

                    return v;
                }

        } operands;

        ScenarioFactors& operator+=(const ScenarioFactors& other)
        {
            operators.scenario += other.operators.scenario;

            operators.constraint += other.operators.constraint;
            operators.event += other.operators.event;
            operators.state += other.operators.state;
            operators.timenode += other.operators.timenode;

            operators.of += other.operators.of;
            operators.after += other.operators.after;
            operators.lbrace += other.operators.lbrace;
            operators.rbrace += other.operators.rbrace;

            operators.expression += other.operators.expression;
            operators.duration += other.operators.duration;

            operands.subprocesses_variables.push_back(other.operands.variables);
            copy(other.operands.subprocesses_variables, operands.subprocesses_variables);

            operands.expressions += other.operands.expressions;
            operands.constraint_rigid_times += other.operands.constraint_rigid_times;
            operands.constraint_minmax_times += other.operands.constraint_minmax_times;

            return *this;
            // TODO -Werror=return-type
        }
};

int sum_unique(const std::vector<int>& vec)
{
    int val = 0;
    for(int e : vec)
    {
        val += int(e > 0);
    }
    return val;
}

int sum_all(const std::vector<int>& vec)
{
    return std::accumulate(vec.begin(), vec.end(), 0);
}

template<class>
class HalsteadVisitor;
template<>
class HalsteadVisitor<Scenario::ScenarioModel>
{
    public:
        const Scenario::ScenarioModel& m_scenar;
        ScenarioFactors f;

        HalsteadVisitor(const Scenario::ScenarioModel& scenar):
            m_scenar{scenar}
        {
            f.operators.lbrace += 1;

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

            f.operators.rbrace += 1;
        }

        template<typename T>
        QString id(const T& c)
        {
            return QString(T::description()) + QString::number(c.id_val());
        }

        void duration(const ConstraintModel& c)
        {
            if(c.duration.isRigid())
            {
                f.operands.constraint_rigid_times += 1;
            }
            else
            {
                f.operands.constraint_minmax_times += 1;
            }
        }

        void visit(const ConstraintModel& c)
        {
            f.operators.constraint += 1;
            f.operands.variables[id(c)] += 1;
            f.operators.after += 1;
            f.operands.variables[id(startState(c, m_scenar))] += 1;

            f.operators.duration += 1;
            f.operators.of += 1;
            f.operands.variables[id(c)] += 1;

            for(auto& process : c.processes)
            {
                if(auto scenar = dynamic_cast<Scenario::ScenarioModel*>(&process))
                {
                    f.operators.scenario += 1;
                    f.operands.variables[id(*scenar)] += 1;
                    f += HalsteadVisitor<Scenario::ScenarioModel>{*scenar}.f;
                    f.operators.of += 1;
                    f.operands.variables[id(c)] += 1;
                }
            }
        }

        void visit(const EventModel& e)
        {
            f.operators.event += 1;
            f.operands.variables[id(e)] += 1;
            f.operators.of += 1;
            f.operands.variables[id(parentTimeNode(e, m_scenar))] += 1;

            if(e.condition().childCount() > 0)
            {
                f.operators.expression += 1;
                f.operands.expressions += 1;
                f.operators.of += 1;
                f.operands.variables[id(e)] += 1;
            }
        }

        void visit(const TimeNodeModel& tn)
        {
            f.operators.timenode += 1;
            f.operands.variables[id(tn)] += 1;

            if(tn.trigger() && tn.trigger()->expression().childCount() > 0)
            {
                f.operators.expression += 1;
                f.operands.expressions += 1;
                f.operators.of += 1;
                f.operands.variables[id(tn)] += 1;
            }
        }

        void visit(const StateModel& st)
        {
            if(st.previousConstraint())
            {
                f.operators.state += 1;
                f.operands.variables[id(st)] += 1;
                f.operators.after += 1;
                f.operands.variables[id(previousConstraint(st, m_scenar))] += 1;
            }

            f.operators.state += 1;
            f.operands.variables[id(st)] += 1;
            f.operators.of += 1;
            f.operands.variables[id(parentEvent(st, m_scenar))] += 1;
        }
};
QString Scenario::Metrics::toScenarioLanguage(
        const Scenario::ScenarioModel& s)
{
    return LanguageVisitor<Scenario::ScenarioModel>{s}.text;
}

Scenario::Metrics::Halstead::Factors
Scenario::Metrics::Halstead::ComputeFactors(const Scenario::ScenarioModel& scenar)
{
    auto sf = HalsteadVisitor<Scenario::ScenarioModel>{scenar}.f;
    Scenario::Metrics::Halstead::Factors factors;
    factors.eta1 = sum_unique(sf.operators.toVector());
    factors.eta2 = sum_unique(sf.operands.toVector());
    factors.N1 = sum_all(sf.operators.toVector());
    factors.N2 = sum_all(sf.operands.toVector());
    return factors;
}
