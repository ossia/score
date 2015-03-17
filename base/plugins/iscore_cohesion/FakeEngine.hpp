#pragma once
#include <QString>
#include <thread>
#include <QObject>

class TTObject;
class FakeEngine : public QObject
{
        Q_OBJECT
    public:
        ~FakeEngine();
        void runScore(QString scoreFilePath);


    signals:
        void currentTimeChanged(double);

    private:
        void runThread(QString filepath);

        std::thread m_thread;
        TTObject* m_scenario{};
};

