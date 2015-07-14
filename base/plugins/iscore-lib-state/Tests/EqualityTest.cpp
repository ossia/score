#include <QTest>
#include <State/State.hpp>
#include <State/Message.hpp>

using namespace iscore;
class EqualityTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void equalityTest()
        {
            qRegisterMetaTypeStreamOperators<Address>();

            QMetaType::registerComparators<Message>();
            QMetaType::registerComparators<MessageList>();
            qRegisterMetaTypeStreamOperators<Message>();
            qRegisterMetaTypeStreamOperators<MessageList>();
            qRegisterMetaTypeStreamOperators<iscore::Value>();
            Message m;
            m.address = {"dada", {"bilou", "yadaa", "zoo"}};
            Q_ASSERT(m == m);

            m.value.val = 5.5;
            Q_ASSERT(m == m);

            MessageList l1;
            l1.push_back(m);

            MessageList l2;
            l2.push_back(m);

            Q_ASSERT(l1 == l2);

            Q_ASSERT(!(l1 != l2));
            State s1 (l1);
            State s2 (l2);
            Q_ASSERT (s1 == s2);
            Q_ASSERT (!(s1 != s2));

            Message m2;
            m.address = {"dodo", {"a", "baba"}};
            m.value.val = 3;
            Q_ASSERT(m != m2);
            MessageList l3;
            l3.push_back(m2);
            Q_ASSERT(l3 != l2);

            State s3(l3);
            Q_ASSERT(s2 != s3);
        }
};

QTEST_MAIN(EqualityTest)
#include "EqualityTest.moc"
