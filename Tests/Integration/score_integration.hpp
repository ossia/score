#pragma once
#include <core/application/MinimalApplication.hpp>

#define SCORE_INTEGRATION_TEST(TestFun)                            \
                                                                   \
int main(int argc, char** argv)                                    \
{                                                                  \
  QLocale::setDefault(QLocale::C);                                 \
  std::setlocale(LC_ALL, "C");                                     \
                                                                   \
  score::MinimalGUIApplication app(argc, argv);                    \
                                                                   \
  QMetaObject::invokeMethod(&app, [] {                             \
    TestFun();                                                     \
    QApplication::processEvents();                                 \
    qApp->exit(0);                                                 \
                                                                   \
  }, Qt::QueuedConnection);                                        \
                                                                   \
  return app.exec();                                               \
}


#define SCORE_INTEGRATION_TEST_OBJECT(TestObject) \
int main(int argc, char** argv) \
{ \
    TestObject tc(argc, argv); \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}

