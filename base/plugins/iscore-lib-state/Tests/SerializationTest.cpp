#include <QTest>
#include <State/State.hpp>
#include <State/Message.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace iscore {
    class State;
}
using namespace iscore;
class SerializationTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void serializationTest()
        {
            using namespace iscore;
            QMetaType::registerComparators<Message>();
            qRegisterMetaTypeStreamOperators<Message>();
            qRegisterMetaTypeStreamOperators<MessageList>();
            qRegisterMetaTypeStreamOperators<iscore::Value>();
            Message m;
            m.address = {"dada", {"bilou", "yadaa", "zoo"}};
            m.value.val = 5.5;

            StateData s(iscore::MessageList{m}, "le state");

            Serializer<JSONObject> js;
            js.readFrom(s);
            qDebug() << js.m_obj;

            StateData s2(s);
            Serializer<JSONObject> js2;
            js2.readFrom(s2);
            qDebug() << js2.m_obj;


            Deserializer<JSONObject> jd(js.m_obj);
            StateData s3;
            jd.writeTo(s3);

            ISCORE_ASSERT(s3.is<MessageList>());
        }
};

QTEST_MAIN(SerializationTest)
#include "SerializationTest.moc"
