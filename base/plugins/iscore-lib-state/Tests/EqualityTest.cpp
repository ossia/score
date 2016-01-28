#include <State/Message.hpp>
#include <QtTest/QtTest>
#include <QMetaType>
#include <QObject>

using namespace iscore;
class EqualityTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void equalityTest()
        {
            using namespace State;
            using namespace iscore;
            qRegisterMetaTypeStreamOperators<State::Address>();

            QMetaType::registerComparators<State::Message>();
            QMetaType::registerComparators<State::MessageList>();
            qRegisterMetaTypeStreamOperators<State::Message>();
            qRegisterMetaTypeStreamOperators<State::MessageList>();
            qRegisterMetaTypeStreamOperators<State::Value>();
            Message m;
            m.address = {"dada", {"bilou", "yadaa", "zoo"}};
            ISCORE_ASSERT(m == m);

            m.value.val = 5.5;
            ISCORE_ASSERT(m == m);

            MessageList l1;
            l1.push_back(m);

            MessageList l2;
            l2.push_back(m);

            ISCORE_ASSERT(l1 == l2);
/*
            ISCORE_ASSERT(!(l1 != l2));
            StateData s1 (l1, "aState");
            StateData s2 (l2, "otherState");
            ISCORE_ASSERT (s1 == s2);
            ISCORE_ASSERT (!(s1 != s2));

            Message m2;
            m.address = {"dodo", {"a", "baba"}};
            m.value.val = 3;
            ISCORE_ASSERT(m != m2);
            MessageList l3;
            l3.push_back(m2);
            ISCORE_ASSERT(l3 != l2);

            StateData s3(l3);
            ISCORE_ASSERT(s2 != s3);
            */
        }
};

QTEST_APPLESS_MAIN(EqualityTest)
#include "EqualityTest.moc"
#include <State/Address.hpp>
#include <State/Value.hpp>
