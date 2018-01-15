// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/model/path/ObjectPath.hpp>

class SerializationTest : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void JSONTest()
  {
    JSONObjectReader reader;
    reader.readFrom(test_path);

    ObjectPath path;
    JSONObjectWriter writer(reader.m_obj);
    writer.writeTo(path);

    QVERIFY(path == test_path);
  }

  void DataStreamTest()
  {
    QByteArray arr;
    DataStreamReader reader(&arr);
    reader.readFrom(test_path);

    ObjectPath path;
    DataStreamWriter writer(&arr);
    writer.writeTo(path);

    QVERIFY(path == test_path);
  }

private:
  const ObjectPath test_path{{"IntervalModel", {}},
                             {"IntervalModel", 0},
                             {"ScenarioProcessSharedModel", 23},
                             {"IntervalModel", -42}};
};

QTEST_MAIN(SerializationTest)
#include "SerializationTest.moc"
