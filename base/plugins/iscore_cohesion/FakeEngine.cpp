#include "FakeEngine.hpp"

#include "TTModular.h"
#include "TTScore.h"
#include <iostream>
#include <QString>
#include <string>


#include <QDir>
#include <QApplication>
#include <QDebug>
#include <thread>

void FakeEngine::runThread(QString scoreFilePath)
{
    TTSymbol filepath {scoreFilePath.toLatin1().constData() };  // .score file to load
    QString jamomaFolder = (QCoreApplication::applicationDirPath() + "/../Frameworks/jamoma");

    if(!QDir(jamomaFolder).exists())
    {
        jamomaFolder = "/usr/local/jamoma";
    }

    // initialisation of Modular environnement (passing the folder path where all the dylibs are)
    TTModularInit(nullptr, true);

    // create an application manager
    TTObject applicationManager("ApplicationManager");

    // create a local application named i-score
    TTObject applicationLocal = applicationManager.send("ApplicationInstantiateLocal", "i-score");

    // loads protocol unit
    // TODO : when parsing project file
    {
        // create Minuit protocol unit
        TTObject protocolMinuit = applicationManager.send("ProtocolInstantiate", "Minuit");

        // create OSC protocol unit
        TTObject protocolOSC = applicationManager.send("ProtocolInstantiate", "OSC");
    }

    // initialisation of Score environnement (passing the folder path where all the dylibs are)
    TTScoreInit(jamomaFolder.toLatin1().constData());

    // create a scenario
    TTObject scenario("Scenario");
    m_scenario = &scenario;

    // load project file
    TTObject xmlHandler("XmlHandler");
    xmlHandler.set("object", TTValue(applicationManager, scenario));
    xmlHandler.send("Read", filepath);

    // run scenario
    scenario.send("Start");

    // wait for scenario
    TTBoolean running;

    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
        TTValue out;
        scenario.get("running", running);
        scenario.get("date", out);

        emit currentTimeChanged( TTFloat64(out[0]));
    }
    while(running);

    m_scenario = nullptr;
}

FakeEngine::~FakeEngine()
{
    if(m_scenario)
    {
        m_scenario->send("Stop");
        m_scenario = nullptr;
        while(!m_thread.joinable())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_thread.join();
    }
}

void FakeEngine::runScore(QString scoreFilePath)
{
    if(m_thread.joinable())
        m_thread.join();
    m_thread = std::thread{&FakeEngine::runThread, this, scoreFilePath};
}

