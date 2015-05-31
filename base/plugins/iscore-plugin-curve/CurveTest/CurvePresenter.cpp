#include "CurvePresenter.hpp"
#include <QFinalState>
#include <QAbstractTransition>
#include <iscore/command/OngoingCommandManager.hpp>
namespace {
template<int N>
struct NumberedEvent : public QEvent
{
        static constexpr const int user_type = N;
        NumberedEvent():
            QEvent{QEvent::Type(QEvent::User + N)} { }
};

template<typename Event>
class MatchedTransition : public QAbstractTransition
{
    public:
        using event_type = Event;
    protected:
        virtual bool eventTest(QEvent *e) override
        { return e->type() == QEvent::Type(QEvent::User + Event::user_type); }

        virtual void onTransition(QEvent *event) override { }
};

template<typename Transition, typename SourceState, typename TargetState, typename... Args>
Transition* make_transition(SourceState source, TargetState dest, Args&&... args)
{
    auto t = new Transition{std::forward<Args>(args)...};
    t->setTargetState(dest);
    source->addTransition(t);
    return t;
}

using Press_Event = NumberedEvent<1>;
using Move_Event = NumberedEvent<2>;
using Release_Event = NumberedEvent<3>;
using Cancel_Event = NumberedEvent<4>;

using Press_Transition = MatchedTransition<Press_Event>;
using Move_Transition = MatchedTransition<Move_Event>;
using Release_Transition = MatchedTransition<Release_Event>;
using Cancel_Transition = MatchedTransition<Cancel_Event>;

/*
class CommandObject
{
    public:
        void instantiate();
        void update();
        void commit();
        void rollback();
};
*/

class OngoingCommandState : public QState
{
    public:
        template<typename CommandObject>
        OngoingCommandState(CommandObject& obj, QState* parent)
        {
            QState* mainState = new QState{this};
            {
                auto pressed = new QState{mainState};
                auto moving = new QState{mainState};
                auto released = new QFinalState{mainState};

                // General setup
                mainState->setInitialState(pressed);

                make_transition<Move_Transition>(pressed, moving); // Also try with pressed, released
                make_transition<Release_Transition>(pressed, released);

                make_transition<Move_Transition>(moving, moving);
                make_transition<Release_Transition>(moving, released);


                connect(pressed, &QAbstractState::entered,
                        this, [&] () {
                    obj.press();
                });
                connect(moving, &QAbstractState::entered,
                        this, [&] () {
                    obj.move();
                });
                connect(released, &QAbstractState::entered,
                        this, [&] () {
                    obj.release();
                });
            }

            auto cancelled = new QState{this};
            auto finalState = new QFinalState{this};

            make_transition<Cancel_Transition>(mainState, cancelled);
            cancelled->addTransition(finalState);

            connect(cancelled, &QAbstractState::entered,
                    this, [&] () {
                obj.cancel();
            });

            setInitialState(mainState);
        }
};

class CurveSegmentFactory
{
    public:
        virtual QString name() const = 0;
        virtual CurveSegmentModel* make(
                const id_type<CurveSegmentModel>&,
                QObject* parent) = 0;
};

class LinearCurveSegmentFactory : public CurveSegmentFactory
{
        // CurveSegmentFactory interface
    public:
        QString name() const
        {
            return "Linear"; // boost hashed_unique will save us
        }

        CurveSegmentModel *make(
                const id_type<CurveSegmentModel>& id,
                QObject* parent)
        {
            return new CurveSegmentLinearModel{id, parent};
        }
};

// Template this
class CurveSegmentList
{
    public:
        CurveSegmentFactory* get(const QString& name)
        {
            return *std::find_if(factories.begin(), factories.end(),
                              [&] (auto&& p) { return p->name() == name; });
        }

        void registration(CurveSegmentFactory* fact)
        {
            factories.push_back(fact);
        }

    private:
        QVector<CurveSegmentFactory*> factories;
};

// CreateSegment
// CreateSegmentBetweenPoints

// RemoveSegment -> easy peasy
// RemovePoint -> which segment do we merge ? At the left or at the right ?
// A point(view) has pointers to one or both of its curve segments.
// TODO do automation where only points are sent.

class CurveCommandObjectBase
{
        CurvePresenter* m_presenter{};
    public:
        CurveCommandObjectBase(CurvePresenter* pres):
            m_presenter{pres}
        {

        }

        void press()
        {
            // Generally, on Press one would save the previous state of the curve.
            m_originalSegments = m_presenter->model()->segments();
            m_originalPress = m_presenter->pressedPoint();

            on_press();
        }

        virtual void on_press() = 0;

    protected:
        QVector<CurveSegmentModel*> m_originalSegments;
        QPointF m_originalPress; // Note : there should be only one per curve...

};

class UpdateCurve : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("UpdateCurve", "UpdateCurve")
    public:
        UpdateCurve(ObjectPath&& model, QVector<CurveSegmentModel*> segments):
            iscore::SerializableCommand("", "", "")
        {
        }

        void undo() override
        {
        }

        void redo() override
        {
        }

        void update(ObjectPath&& model, QVector<CurveSegmentModel*> segments)
        {

        }

    protected:
        void serializeImpl(QDataStream &) const override
        {
        }

        void deserializeImpl(QDataStream &) override
        {
        }
};

class MovePointCommandObject : public CurveCommandObjectBase
{
        CurvePresenter* m_presenter{};
    public:
        MovePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack):
            CurveCommandObjectBase(nullptr),
            m_dispatcher{stack}
        {

        }

        void on_press() override
        {
            // Create the command. For now nothing changes.
            m_dispatcher.submitCommand<UpdateCurve>(iscore::IDocument::path(m_presenter->model()),
                                                    m_originalSegments);

        }

        void move()
        {

        }

        void release()
        {

        }

        void cancel()
        {

        }

    private:
        SingleOngoingCommandDispatcher m_dispatcher;
};

// To simplify :
// Take the current state of the curve
// Compute the state we have to be in
// Make a command that sets a new state for the curve.


// Will move the segment and potentially the start point of the next segment and the end point of the previous segment.
// Or does the command do this ?
// How to find the previous - next segments ? They have to be linked...
// How to prevent overlapping segments ?
// Can move create new segments ? I'd say no.
class MoveSegmentCommandObject
{
    public:
        MoveSegmentCommandObject(iscore::CommandStack& stack):
            m_dispatcher{stack}
        {

        }

        void press()
        {

        }

        void move()
        {

        }

        void release()
        {

        }

        void cancel()
        {

        }

    private:
        SingleOngoingCommandDispatcher m_dispatcher;
};
}

CurvePresenter::CurvePresenter(CurveModel* model, CurveView* view):
    m_model{model},
    m_view{view}
{

}

