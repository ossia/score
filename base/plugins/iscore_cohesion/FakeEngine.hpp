#pragma once
#include <QString>
#include <thread>
#include <QObject>

class FakeEngine : public QObject
{
        Q_OBJECT
    public:
        void runScore(QString scoreFilePath);

    signals:
        void currentTimeChanged(double);

    private:
        void runThread(QString filepath);

        std::thread m_thread;
};

