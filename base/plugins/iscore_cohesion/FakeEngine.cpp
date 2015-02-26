#include "FakeEngine.hpp"
/** @file
 *
 * @ingroup scoreImplementation
 *
 * @brief a command line tool to load and play a .score file format
 *
 * @details ... @n@n
 *
 * @see TTScore, TTModular
 *
 * @author Théo de la Hogue
 *
 * @copyright Copyright © 2014, Théo de la Hogue @n
 * This code is licensed under the terms of the "CeCILL-C" @n
 * http://www.cecill.info
 */

#include "TTModular.h"
#include "TTScore.h"
#include <source/Control/OldFormatConversion.hpp>
#include <source/Document/BaseElement/BaseElementModel.hpp>
#include <iostream>
#include <QString>
#include <string>
#include <QTemporaryFile>
#include <QDir>
#include <QApplication>

void runScore(QString scoreFilePath);

void FakeEngineExecute()
{
    //TODO pass it in argument.
    auto doc = qApp->findChild<BaseElementModel*> ("BaseElementModel");
    auto data = JSONToZeroTwo(doc->toJson());

    QTemporaryFile f;

    if(f.open())
    {
        f.write(data.toLatin1().constData(), data.size());
        f.flush();
        runScore(f.fileName());
    }
}

void runScore(QString scoreFilePath)
{
    TTSymbol filepath {scoreFilePath.toLatin1().constData() };  // .score file to load

    QString jamomaFolder = (QCoreApplication::applicationDirPath() + "/../Frameworks/jamoma");

    if(!QDir(jamomaFolder).exists())
    {
        jamomaFolder = "/usr/local/jamoma";
    }

    // initialisation of Modular environnement (passing the folder path where all the dylibs are)
    TTModularInit(jamomaFolder.toLatin1().constData());

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
        sleep(1);
        scenario.get("running", running);
    }
    while(running);
}

