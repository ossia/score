// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QtTest/QtTest>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/path/ObjectPath.hpp>

class SerializationTest : public QObject
{
  Q_OBJECT

private slots:
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
  const ObjectPath test_path{{"ConstraintModel", {}},
                             {"ConstraintModel", 0},
                             {"ScenarioProcessSharedModel", 23},
                             {"ConstraintModel", -42}};
};

QTEST_MAIN(SerializationTest)
#include "SerializationTest.moc"
