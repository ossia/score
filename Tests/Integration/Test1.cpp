
#include <QtWidgets>
#include <QtTest/QtTest>
#include <core/application/Application.hpp>
class Test1: public QObject
{
    Q_OBJECT

private slots:
    void testGui()
    {
        int argc = 0;
        iscore::Application app{argc, nullptr};
        QLineEdit lineEdit;

        QTest::keyClicks(&lineEdit, "hello world");

        QCOMPARE(lineEdit.text(), QString("hello world"));
        qApp->postEvent(0, 0);
        app.exec();
    }

};

QTEST_MAIN(Test1)
#include "Test1.moc"
