/*
Copyright: LaBRI / SCRIME

Authors : Jaime Chao, Clément Bossut (2013-2014)

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software.  You can  use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and,  more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/

#include "engine.hpp"

Engine::Engine()
  : _mainScenario(NULL)
{
  initModular();
  initScore();
}

void Engine::initModular()
{
    TTErr           err;
    TTValue         args, v;
    TTString        applicationName = "i-score";                  // TODO : declare as global variable
    TTObjectBasePtr iscore;                                       // TODO : declare as global variable
    TTString        configFile = "/usr/local/include/IScore/i-scoreConfiguration.xml";
    TTHashPtr       hashParameters;

    // this initializes the Modular framework and loads protocol plugins (in /usr/local/jamoma/extensions folder)
    TTModularInit();

    // create a local application named i-score
    TTModularCreateLocalApplication(applicationName, configFile);

    // get i-score application
    iscore = getLocalApplication;

    // set the application in debug mode
    iscore->setAttributeValue(TTSymbol("debug"), YES);

    // create TTData for tranport service and expose them
    // Not done for the moment (see Engine.cpp of i-score)

    ////////////
    // Example : Create a distant application
    ////////////
    TTObjectBasePtr anApplication = NULL;
    TTSymbol        MinuitApplicationName;

    // create an application called MinuitDevice1
    MinuitApplicationName = TTSymbol("MinuitDevice1");
    args = TTValue(MinuitApplicationName);
    TTObjectBaseInstantiate(kTTSym_Application, TTObjectBaseHandle(&anApplication), args);

    // set application type : here 'mirror' because it use Minuit protocol
    anApplication->setAttributeValue(kTTSym_type, TTSymbol("mirror"));

    ////////////
    // Example : Register i-score and MinuitDevice1 to the Minuit protocol
    ////////////

    // check if the Minuit protocol has been loaded
    if (getProtocol(TTSymbol("Minuit"))) {

        // register the local application to the Minuit protocol
        getProtocol(TTSymbol("Minuit"))->sendMessage(TTSymbol("registerApplication"), TTSymbol(applicationName), kTTValNONE);

        // get parameter's table
        v = TTSymbol(applicationName);
        err = getProtocol(TTSymbol("Minuit"))->getAttributeValue(TTSymbol("applicationParameters"), v);

        if (!err) {

            hashParameters = TTHashPtr((TTPtr)v[0]);

            // replace the Minuit parameters for the local application
            hashParameters->remove(TTSymbol("port"));
            hashParameters->append(TTSymbol("port"), 8002);

            hashParameters->remove(TTSymbol("ip"));
            hashParameters->append(TTSymbol("ip"), TTSymbol("127.0.0.1"));

            v = TTSymbol(applicationName);
            v.append((TTPtr)hashParameters);
            getProtocol(TTSymbol("Minuit"))->setAttributeValue(TTSymbol("applicationParameters"), v);
        }

        // register this application to the Minuit protocol
        getProtocol(TTSymbol("Minuit"))->sendMessage(TTSymbol("registerApplication"), MinuitApplicationName, kTTValNONE);

        // get parameter's table
        v = TTSymbol(MinuitApplicationName);
        err = getProtocol(TTSymbol("Minuit"))->getAttributeValue(TTSymbol("applicationParameters"), v);

        if (!err) {

            hashParameters = TTHashPtr((TTPtr)v[0]);

            // replace the Minuit parameters for the distant application
            hashParameters->remove(TTSymbol("port"));
            hashParameters->append(TTSymbol("port"), 9998);

            hashParameters->remove(TTSymbol("ip"));
            hashParameters->append(TTSymbol("ip"), TTSymbol("127.0.0.1"));

            v = TTValue(MinuitApplicationName);
            v.append((TTPtr)hashParameters);
            getProtocol(TTSymbol("Minuit"))->setAttributeValue(TTSymbol("applicationParameters"), v);
        }

        // run the Minuit protocol
        TTModularApplications->sendMessage(TTSymbol("ProtocolRun"), TTSymbol("Minuit"), kTTValNONE);

        ////////////
        // Example : Build the namespace of MinuitDevice1 using discovery feature (if the protocol provides it)
        ////////////

        // you can create an OSC receive on the 9998 port to see that a namespace request is sent by i-score (using Pure Data for example)
        //anApplication->sendMessage(TTSymbol("DirectoryBuild")); //envoit des Minuit::SendDiscoverRequest à tire la rigo
    }
/*
    ////////////
    // Example : Read the namespace of MinuitDevice1 from a namespace file
    ////////////

    // create a TTXmlHandler class to parse a xml namespace file
    TTXmlHandlerPtr myXmlHandler = NULL;
    TTObjectBaseInstantiate(kTTSym_XmlHandler, TTObjectBaseHandle(&myXmlHandler), kTTValNONE);

    // prepare the TTXmlHandler to pass the result of the parsing to MinuitDevice1
    v = TTValue(anApplication);
    myXmlHandler->setAttributeValue(TTSymbol("object"), v);

    // read a namespace file
    err = myXmlHandler->sendMessage(TTSymbol("Read"), TTSymbol("/Users/WALL-E/Documents/Jamoma/Modules/Modular/implementations/MaxMSP/jcom.modular/remoteApp - namespace.xml"), kTTValNONE);

    if (!err) {
        // get the root of the TNodeDirectory of MinuitDevice1
        // note : the TTNodeDirectory is a tree structure used to registered and retrieve TTObjects using TTAddress
        TTNodeDirectoryPtr anApplicationDirectory = getApplicationDirectory(TTSymbol("MinuitDevice1"));

        // return all addresses of the TTNodeDirectory
        std::cout << "__ Dump MinuitDevice1 directory __" << std::endl;

        // start to dump addresses recursilvely from the root of the directory
        dumpAddressBelow(anApplicationDirectory->getRoot());

        std::cout << "__________________________________" << std::endl;
    }
*/
}

void Engine::initScore()
{
    TTTimeEventPtr  startEvent = NULL;
    TTTimeEventPtr  endEvent = NULL;
    TTValue         args;

    // this initializes the Score framework
    TTScoreInit();

    // Create a time event for the start
    args = TTUInt32(0);
    TTObjectBaseInstantiate(TTSymbol("TimeEvent"), TTObjectBaseHandle(&startEvent), args);

    // Create a time event for the end
    args = TTUInt32(60);
    TTObjectBaseInstantiate(TTSymbol("TimeEvent"), TTObjectBaseHandle(&endEvent), args);

    // Create the main scenario
    args = TTObjectBasePtr(startEvent);
    args.append(TTObjectBasePtr(endEvent));
    TTObjectBaseInstantiate(TTSymbol("Scenario"), TTObjectBaseHandle(&_mainScenario), args);

}
