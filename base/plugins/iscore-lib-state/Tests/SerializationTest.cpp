#include <State/Message.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <QtTest/QtTest>
#include <QMetaType>
#include <QObject>

using namespace iscore;
class SerializationTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void serializationTest()
        {
            using namespace iscore;
            QMetaType::registerComparators<State::Message>();
            qRegisterMetaTypeStreamOperators<State::Message>();
            qRegisterMetaTypeStreamOperators<State::MessageList>();
            qRegisterMetaTypeStreamOperators<State::Value>();
            State::Message m;
            m.address = {"dada", {"bilou", "yadaa", "zoo"}};
            m.value.val = 5.5f;

            {
                auto json = marshall<JSONObject>(m);
                auto mess_json = unmarshall<State::Message>(json);
                ISCORE_ASSERT(m == mess_json);

                auto barray = marshall<DataStream>(m);
                auto mess_array = unmarshall<State::Message>(barray);
                ISCORE_ASSERT(m == mess_array);

            }
        }
};

QTEST_APPLESS_MAIN(SerializationTest)
#include "SerializationTest.moc"
#include <State/Address.hpp>
#include <State/Value.hpp>

class DataStream;
class JSONObject;
