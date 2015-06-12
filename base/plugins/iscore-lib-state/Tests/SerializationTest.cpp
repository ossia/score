#include <QTest>
#include <State/State.hpp>
#include <State/Message.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


class SerializationTest: public QObject
{
        Q_OBJECT
    public:

    private slots:

        void serializationTest()
        {
            QMetaType::registerComparators<Message>();
            qRegisterMetaTypeStreamOperators<Message>();
            qRegisterMetaTypeStreamOperators<MessageList>();
            Message m;
            m.address = {"dada", {"bilou", "yadaa", "zoo"}};
            m.value = 5.5;

            State s(m);

            Serializer<JSONObject> js;
            js.readFrom(s);
            qDebug() << js.m_obj;

            State s2(s);
            Serializer<JSONObject> js2;
            js2.readFrom(s2);
            qDebug() << js2.m_obj;


            Deserializer<JSONObject> jd(js.m_obj);
            State s3;
            jd.writeTo(s3);

            Q_ASSERT(s3.data().canConvert<Message>());
        }
};

QTEST_MAIN(SerializationTest)
#include "SerializationTest.moc"
