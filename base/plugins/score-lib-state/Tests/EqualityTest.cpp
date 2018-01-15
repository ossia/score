// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QMetaType>
#include <QObject>
#include <QtTest/QtTest>
#include <State/Message.hpp>

using namespace score;
class EqualityTest : public QObject
{
  Q_OBJECT
public:
private Q_SLOTS:

  void equalityTest()
  {
    using namespace State;
    using namespace score;
    qRegisterMetaTypeStreamOperators<State::Address>();

    QMetaType::registerComparators<State::Message>();
    QMetaType::registerComparators<State::MessageList>();
    qRegisterMetaTypeStreamOperators<State::Message>();
    qRegisterMetaTypeStreamOperators<State::MessageList>();
    qRegisterMetaTypeStreamOperators<ossia::value>();
    Message m;
    m.address = {"dada", {"bilou", "yadaa", "zoo"}};
    SCORE_ASSERT(m == m);

    m.value = 5.5f;
    SCORE_ASSERT(m == m);

    MessageList l1;
    l1.push_back(m);

    MessageList l2;
    l2.push_back(m);

    SCORE_ASSERT(l1 == l2);
    /*
                SCORE_ASSERT(!(l1 != l2));
                StateData s1 (l1, "aState");
                StateData s2 (l2, "otherState");
                SCORE_ASSERT (s1 == s2);
                SCORE_ASSERT (!(s1 != s2));

                Message m2;
                m.address = {"dodo", {"a", "baba"}};
                m.value.val = 3;
                SCORE_ASSERT(m != m2);
                MessageList l3;
                l3.push_back(m2);
                SCORE_ASSERT(l3 != l2);

                StateData s3(l3);
                SCORE_ASSERT(s2 != s3);
                */
  }
};

QTEST_APPLESS_MAIN(EqualityTest)
#include "EqualityTest.moc"
#include <State/Address.hpp>
#include <State/Value.hpp>
