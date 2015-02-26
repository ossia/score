#include <QtTest/QtTest>

class TestExample: public QObject
{
        Q_OBJECT
    private slots:
        void SomeTest()
        {
            QVERIFY(1 < 2);
        }
};

QTEST_MAIN(TestExample)
#include "main.moc"
