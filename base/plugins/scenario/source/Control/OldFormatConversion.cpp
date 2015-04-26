#include <QCoreApplication>
#include <QDomDocument>
#include <QFile>
#include <QTextStream>
#include <QDebug>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


// séparateur décimal = virgule ! Et virgule *obligatoire* (1,0 et non 1)
QString numberToQString(double nb)
{
    QString str = QLocale("fr_FR").toString(nb);
    if (str.indexOf(",") == -1)
    {
        str += QString(",0");
    }
    return str;
}

void createDeviceTree(QJsonArray children, QDomElement* parentNode, QDomDocument* doc)
{
    for(auto child : children)
    {
        QDomElement dom_devNode = doc->createElement("node");
        dom_devNode.setAttribute("address", child.toObject() ["Name"].toString());
        dom_devNode.setAttribute("object", "Data");
        dom_devNode.setAttribute("service", "parameter");
        dom_devNode.setAttribute("dataspace", "none");
        dom_devNode.setAttribute("type", "generic");
        dom_devNode.setAttribute("valueStepsize", "0,100000");
        dom_devNode.setAttribute("priority", "0");
        dom_devNode.setAttribute("rangeClipmode", "none");
        dom_devNode.setAttribute("valueDefault", "0,000000");
        dom_devNode.setAttribute("rangeBounds", "0. 1.");
        dom_devNode.setAttribute("tags", "0");
        dom_devNode.setAttribute("repetitionsFilter", "0");
        dom_devNode.setAttribute("dataspaceUnit", "none");
        dom_devNode.setAttribute("rampDrive", "none");
        dom_devNode.setAttribute("active", "1");
        dom_devNode.setAttribute("rampFunction", "none");
        parentNode->appendChild(dom_devNode);

        createDeviceTree(child.toObject() ["Children"].toArray(), &dom_devNode, doc);
    }
}

QString JSONToZeroTwo(QJsonObject base)
{
    //QCoreApplication a(argc, argv);

    QDomDocument domdoc("i-score0.2");
    QDomProcessingInstruction xmlDeclaration = domdoc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ");
    domdoc.appendChild(xmlDeclaration);

    ////////////////////////////////////////////////
    // default nodes, pour mémoire et pour copier-coller
    ////////////////////////////////////////////////
    /*/

      // appli device
        QDomElement dom_devAppli = domdoc.createElement("application");
        dom_appli.setAttribute("name", "oscDevice");
        dom_appli.setAttribute("author", "");
        dom_appli.setAttribute("version", "");
        dom_appli.setAttribute("type", "proxy");
            QDomElement dom_devNode = domdoc.createElement(("node");
            dom_devNode.setAttribute("address", "");
            dom_devNode.setAttribute("object", "Data");
            dom_devNode.setAttribute("service", "parameter");
            dom_devNode.setAttribute("dataspace", "none");
            dom_devNode.setAttribute("type", "generic");
            dom_devNode.setAttribute("valueStepsize", "0,100000");
            dom_devNode.setAttribute("priority", "0");
            dom_devNode.setAttribute("rangeClipmode", "none");
            dom_devNode.setAttribute("valueDefault", "0,000000");
            dom_devNode.setAttribute("rangeBounds", "0. 1.");
            dom_devNode.setAttribute("tags", "O");
            dom_devNode.setAttribute("repetitionsFilter", "0");
            dom_devNode.setAttribute("dataspaceUnit", "none");
            dom_devNode.setAttribute("rampDrive", "none");
            dom_devNode.setAttribute("active", "1");
            dom_devNode.setAttribute("rampFunction", "none");

        // dans Application i-score
        QDomElement AutomationNode = domdoc.createElement("node");
        AutomationNode.setAttribute("address", "boxName");
        AutomationNode.setAttribute("object", "Automation");

        QDomElement TriggerNode = domdoc.createElement("node");
        TriggerNode.setAttribute("address", "expression");
        TriggerNode.setAttribute("object", "TimeCondition");

        // dans scenario


        QDomElement DefaultEvent = domdoc.createElement("event");
        DefaultEvent.setAttribute("name", "j0");
        DefaultEvent.setAttribute("date", "0u");
        DefaultEvent.setAttribute("mute", "0");
        DefaultEvent.setAttribute("condition", "j0"); // if needed

            QDomElement DefaultCommand = domdoc.createElement("command");
            DefaultCommand.setAttribute("address", "device:/path");
                QDomText value = domdoc.createTextNode("value");
                DefaultCommand.appendChild(value);

        QDomElement DefaultAutomation = domdoc.createElement("Automation");
        DefaultAutomation.setAttribute("name","id");
        DefaultAutomation.setAttribute("start","startEventId");
        DefaultAutomation.setAttribute("end","endEventId");
        DefaultAutomation.setAttribute("durationMin","0u");
        DefaultAutomation.setAttribute("durationMax","1u");
        DefaultAutomation.setAttribute("mute","0");
        DefaultAutomation.setAttribute("verticalPosition","0u");
        DefaultAutomation.setAttribute("verticalSize","1u");

            QDomElement DefaultIndex = domdoc.createElement("indexedCurves");
            DefaultIndex.setAttribute("address", "device:/path");

                QDomElement DefaultCurve = domdoc.createElement("curve");
                DefaultCurve.setAttribute("active","1");
                DefaultCurve.setAttribute("redundancy","0");
                DefaultCurve.setAttribute("sampleRate","40u");
                DefaultCurve.setAttribute("function","0 0 1"); // WARNING : virgule comme separateur decimal !!

        QDomElement DefaultCondition = domdoc.createElement("condition");
        DefaultCondition.setAttribute("name", "jxxx");
        DefaultCondition.setAttribute("dispose", "");

            QDomElement DefaultCase = domdoc.createElement("case");
            DefaultCase.setAttribute("event", "eventId");
            DefaultCase.setAttribute("trigger", "expression");
            DefaultCase.setAttribute("default", "0");
            DefaultCondition.appendChild(DefaultCase);
    //*/

    ////////////////////////////////////////////////////////////////
    // Structure globale, toujours là

    // jamoma
    QDomElement dom_root = domdoc.createElement("jamoma");
    dom_root.setAttribute("version", "0.6");
    domdoc.appendChild(dom_root);

    //protocols
    QDomElement midi = domdoc.createElement("protocol");
    midi.setAttribute("name", "MIDI");
    dom_root.appendChild(midi);

    QDomElement osc = domdoc.createElement("protocol");
    osc.setAttribute("name", "OSC");
    dom_root.appendChild(osc);
    /*            QDomElement oscDev = domdoc.createElement("OSCdevice");
                oscDev.setAttribute("ip", "127.0.0.1");
                oscDev.setAttribute("port", "9997u 9996u");
                osc.appendChild(oscDev);
    */
    QDomElement iscore = domdoc.createElement("i-score");
    iscore.setAttribute("ip", "127.0.0.1");
    iscore.setAttribute("port", "13580");
    osc.appendChild(iscore);

    QDomElement minuit = domdoc.createElement("protocol");
    minuit.setAttribute("name", "Minuit");
    dom_root.appendChild(minuit);

    iscore = domdoc.createElement("i-score");
    iscore.setAttribute("ip", "127.0.0.1");
    iscore.setAttribute("port", "13579");
    minuit.appendChild(iscore);

    // application device
    QDomElement dom_devAppli = domdoc.createElement("application");
    dom_devAppli.setAttribute("name", "OSCdevice");
    dom_devAppli.setAttribute("author", "");
    dom_devAppli.setAttribute("version", "");
    dom_devAppli.setAttribute("type", "proxy");
    dom_root.appendChild(dom_devAppli);

    // application i-score
    QDomElement dom_appli = domdoc.createElement("application");
    dom_appli.setAttribute("name", "i-score");
    dom_appli.setAttribute("author", "");
    dom_appli.setAttribute("version", "");
    dom_appli.setAttribute("type", "local");
    dom_root.appendChild(dom_appli);
    QDomElement dom_node_scenario = domdoc.createElement("node");
    dom_node_scenario.setAttribute("address", "Main");
    dom_node_scenario.setAttribute("object", "Scenario");
    dom_appli.appendChild(dom_node_scenario);

    // scenarios
    QDomElement dom_scenar = domdoc.createElement("Scenario");
    dom_scenar.setAttribute("name", "Main");
    dom_scenar.setAttribute("durationMin", "0u");
    dom_scenar.setAttribute("durationMax", "0u");
    dom_scenar.setAttribute("mute", "0");
    dom_scenar.setAttribute("color", "255 0 100");
    dom_scenar.setAttribute("version", "0.3");
    dom_scenar.setAttribute("viewZoom", "0,125000 1,000000");
    dom_scenar.setAttribute("viewPosition", "0 0");
    dom_root.appendChild(dom_scenar);

    QDomElement start = domdoc.createElement("startEvent");
    start.setAttribute("name", "start");
    start.setAttribute("date", "0u");
    start.setAttribute("mute", "0");
    dom_scenar.appendChild(start);

    QDomElement dom_end = domdoc.createElement("endEvent");
    dom_end.setAttribute("name", "end");
    dom_end.setAttribute("date", "36000000u");
    dom_end.setAttribute("mute", "0");
    dom_scenar.appendChild(dom_end);

    ////////////////////////////////////////////////////////////
    // REMPLISSAGE
    ////////////////////////////////////////////////////////////

    // coeff pour ajuster la mise à l'échelle
    const int TIMECOEFF = 1;
    const int YCOEFF = 2000;

    /*
     besoin de créer des faux events :
        dans 0.2 les boîtes sont forcément reliées par une relation non nulle.
        On ne peut donc pas enchainer deux boîtes directement.

    Solution : on scinde artificiellement l'event de transition en 2
               on insère un intervalle minimal entre les 2

    problème aussi pour les convergences
    */

    int idIndent = 10; // utilisé pour créer des identifiants des nouveaux "fakes" events
    const int DELTAT = 500; // translate tout le scénario (évite le GRIP du début)
    int deltaDuration = 1; // durée de l'intervalle artificiel


    QMap<int, QString> expression;


    //////////////////////////////////////////////////////////////////////////////
    // BOUCLE POUR LES DEVICES
    //////////////////////////////////////////////////////////////////////////////

    QJsonArray j_root = base["DeviceExplorerPanelModel"].toObject() ["Children"].toArray();

    for(auto dev : j_root)
    {
        QString protocol = QString("OSC"); //device[0].toString();
        QString devName = dev.toObject()["Name"].toString();
        dom_devAppli.setAttribute("name", devName);

        if(protocol == "OSC")
        {
            auto devSettings = dev.toObject()["DeviceSettings"].toObject();
            QString port = QString::number(devSettings["InputPort"].toInt()) + QString("u ");
            port += QString::number(devSettings["OutputPort"].toInt()) + QString("u");

            QString host = devSettings["Host"].toString();

            QDomElement oscDev = domdoc.createElement(devName);
             oscDev.setAttribute("ip", host);
            oscDev.setAttribute("port", port);
            osc.appendChild(oscDev);
        }


        QDomElement parentNode = dom_devAppli;
        QJsonArray children = dev.toObject() ["Children"].toArray();

        createDeviceTree(children, &parentNode, &domdoc);

    }


    QJsonObject scenar = base["Document"].toObject();
    QJsonObject mainCstr = scenar["Constraint"].toObject();

    QJsonArray processes = mainCstr["Processes"].toArray();

    for(auto process : processes)
    {
        auto j_proc = process.toObject();

        if(j_proc["ProcessName"].toString() == "Scenario")
        {
            qDebug() << "iterating on scenario";
            //////////////////////////////////////////////////////////////////////////////
            // BOUCLE POUR LES EVENTS
            //////////////////////////////////////////////////////////////////////////////

            QJsonArray events = j_proc["Events"].toArray();

            for(auto event : events)
            {
                auto j_ev = event.toObject();
                auto idNode = j_ev["Identifier"].toObject();

                QString id = "j"; // identifiant 0.2 commencent par un j ...
                id += QString::number(idNode["IdentifierValue"].toInt());

                QString date = QString::number(int(j_ev["Date"].toObject() ["Time"].toDouble() / TIMECOEFF + DELTAT));
                date += "u";    // position 0.2 finissent par u ...

                // dans le noeud "Scenario"
                QDomElement dom_event = domdoc.createElement("event");
                dom_event.setAttribute("name", id);
                dom_event.setAttribute("date", date);
                dom_event.setAttribute("mute", "0");
                dom_scenar.appendChild(dom_event);

                QString expr = j_ev["Condition"].toString();
                expr.remove(0, 1);
                expr.insert(expr.indexOf("/"), ":");

                expression[idNode["IdentifierValue"].toInt()] = expr;

                auto states = j_ev["States"].toArray();

                if(states.size())
                {
                    for(auto state : states)
                    {
                        auto j_rootState = state.toObject();
                        qDebug() << j_rootState;
                        auto j_msg = j_rootState["Data"].toObject();

                        QString msg = j_msg["Address"].toString();
                        qDebug() << msg;
                        QString address = msg;
                        address.truncate(msg.indexOf(" "));
                        address.remove(0, 1);
                        address.insert(address.indexOf("/"), ":");

                        QString value = msg.remove(0, address.length() + 1);

//                       address = "oscDevice:" + address;
                        QDomElement dom_Command = domdoc.createElement("command");
                        dom_Command.setAttribute("address", address);
                        QDomText dom_value = domdoc.createTextNode(value);
                        dom_Command.appendChild(dom_value);
                        dom_event.appendChild(dom_Command);

                    }
                }

                /*
                QDomElement DefaultCondition = domdoc.createElement("condition");
                DefaultCondition.setAttribute("name", "jxxx");
                DefaultCondition.setAttribute("dispose", "");

                QDomElement DefaultCase = domdoc.createElement("case");
                DefaultCase.setAttribute("event", "eventId");
                DefaultCase.setAttribute("trigger", "expression");
                DefaultCase.setAttribute("default", "0");
                DefaultCondition.appendChild(DefaultCase);
                */
            }

            //////////////////////////////////////////////////////////////////////////////
            // BOUCLE POUR LES CONSTRAINTES
            //////////////////////////////////////////////////////////////////////////////

            QJsonArray constraints = j_proc["Constraints"].toArray();

            QMap<int, int> endEventUsed;

            for(auto constraint : constraints)
            {
                int convergence = 1; // permet de faire les convergences
                auto j_cstr = constraint.toObject();
                auto idNode = j_cstr["Identifier"].toObject();
                auto endEvent = j_cstr["EndEvent"].toObject();
                auto startEvent = j_cstr["StartEvent"].toObject();

                int endEv = endEvent["IdentifierValue"].toInt();
                int boxEndEv = endEv;

                if(! endEventUsed.contains(endEv))
                {
                    int endEventDate = int(j_cstr["StartDate"].toObject() ["Time"].toDouble() + j_cstr["DefaultDuration"].toObject()["Time"].toDouble());
                    endEventUsed[endEv] = (endEventDate / TIMECOEFF + DELTAT);
                }
                else
                {
                    boxEndEv -= idIndent;
                    idIndent++;
                    convergence = 2;
                }

                QString boxName = "box_";
                boxName += QString::number(idNode["IdentifierValue"].toInt());

                QString interStart = "j";
                interStart += QString::number(startEvent["IdentifierValue"].toInt());

                QString boxEnd = "j";
                boxEnd += QString::number(boxEndEv);

                QString interEnd = "j";
                interEnd += QString::number(endEv);


                QString min = QString::number(int(j_cstr["MinDuration"].toObject() ["Time"].toDouble() / TIMECOEFF - deltaDuration * convergence));
                min += "u";
                QString max = QString::number(int(j_cstr["MaxDuration"].toObject() ["Time"].toDouble() / TIMECOEFF - deltaDuration * convergence));
                max += "u";

                QString y = QString::number(int (j_cstr["HeightPercentage"].toDouble() * YCOEFF));
                y += "u";

                /* **********************************************************************
                ** event artificiel du début : on change son id et on le décale en temps
                ** **********************************************************************/

                QString boxStart = interStart;
                boxStart += QString::number(idIndent);
                idIndent++;
                QString startDate = QString::number(int(j_cstr["StartDate"].toObject() ["Time"].toDouble() / TIMECOEFF + DELTAT + deltaDuration));
                startDate += "u";

                QDomElement dom_event = domdoc.createElement("event");
                dom_event.setAttribute("name", boxStart);
                dom_event.setAttribute("date", startDate);
                dom_event.setAttribute("mute", "0");
                // ne doit pas prendre de condition (condition sur l'event de fin de la contrainte précédente)
                dom_scenar.appendChild(dom_event);

                /* **********************************************************************
                ** event artificiel de fin (si convergence) : on change son id et on le décale en temps
                ** **********************************************************************/

                if (convergence == 2)
                {
                    QString endDate = QString::number(endEventUsed.value(endEv) - deltaDuration);
                    endDate += "u";

                    QDomElement dom_endEvent = domdoc.createElement("event");
                    dom_endEvent.setAttribute("name", boxEnd);
                    dom_endEvent.setAttribute("date", endDate);
                    dom_endEvent.setAttribute("mute", "0");
                    dom_scenar.appendChild(dom_endEvent);
                }


                QDomElement dom_box = domdoc.createElement("Automation");
                dom_box.setAttribute("name", boxName);
                dom_box.setAttribute("start", boxStart);
                dom_box.setAttribute("end", boxEnd);
                dom_box.setAttribute("durationMin", min);
                dom_box.setAttribute("durationMax", max);
                dom_box.setAttribute("mute", "0");
                dom_box.setAttribute("color", "60 60 60");
                dom_box.setAttribute("verticalPosition", y);
                dom_box.setAttribute("verticalSize", "150u");
                dom_scenar.appendChild(dom_box);

                QDomElement dom_subscenar = dom_box.cloneNode().toElement();
                dom_subscenar.setTagName("Scenario");
                dom_subscenar.setAttribute("name", "scenar");
                dom_subscenar.setAttribute("color", "255 255 255");
                dom_scenar.appendChild(dom_subscenar);

                /* ***********************************************************************
                 * Courbes
                 * ***********************************************************************/

                QJsonArray cstrProcesses = j_cstr["Processes"].toArray();

                for(auto cstrProcess : cstrProcesses)
                {
                    auto autom = cstrProcess.toObject();

                    if(autom["ProcessName"].toString() == "Automation")
                    {
                        QString automAddress = autom["Address"].toString();
                        automAddress.remove(0, 1);
                        automAddress.insert(automAddress.indexOf("/"), ":");

                        QString points;

                        for(auto point : autom["Points"].toArray())
                        {
                            points += numberToQString(point.toObject() ["k"].toDouble());

                            points += " ";
                            points += numberToQString(point.toObject() ["v"].toDouble());
                            points += " ";
                            points += "1,000000 ";

                        }

                        QDomElement DefaultIndex = domdoc.createElement("indexedCurves");
                        DefaultIndex.setAttribute("address", automAddress);

                        QDomElement DefaultCurve = domdoc.createElement("curve");
                        DefaultCurve.setAttribute("active", "1");
                        DefaultCurve.setAttribute("redundancy", "0");
                        DefaultCurve.setAttribute("sampleRate", "40u");
                        DefaultCurve.setAttribute("function", points);

                        DefaultIndex.appendChild(DefaultCurve);
                        dom_box.appendChild(DefaultIndex);

                    }
                }

                /* **********************************************************************
                **  intervalle le cas échéant
                ** **********************************************************************/

                // pas d'intervalle à partir de l'origine
                if(startEvent["IdentifierValue"].toInt())
                {
                    QDomElement dom_interval = domdoc.createElement("Interval");
                    dom_interval.setAttribute("name", "interval");
                    dom_interval.setAttribute("start", interStart);
                    dom_interval.setAttribute("end", boxStart);
                    dom_interval.setAttribute("durationMin", "1u");
                    dom_interval.setAttribute("durationMax", "1u");
                    dom_interval.setAttribute("mute", "0");
                    dom_interval.setAttribute("color", "255 255 255");
                    dom_interval.setAttribute("verticalPosition", "0u");
                    dom_interval.setAttribute("verticalSize", "1u");
                    dom_scenar.appendChild(dom_interval);
                }
                // si convergence, intervalle à la fin
                if(convergence == 2)
                {
                    QDomElement dom_interval = domdoc.createElement("Interval");
                    dom_interval.setAttribute("name", "interval");
                    dom_interval.setAttribute("start", boxEnd);
                    dom_interval.setAttribute("end", interEnd);
                    dom_interval.setAttribute("durationMin", "1u");
                    dom_interval.setAttribute("durationMax", "1u");
                    dom_interval.setAttribute("mute", "0");
                    dom_interval.setAttribute("color", "255 255 255");
                    dom_interval.setAttribute("verticalPosition", "0u");
                    dom_interval.setAttribute("verticalSize", "1u");
                    dom_scenar.appendChild(dom_interval);
                }

                // déclaration de la boîte dans le noeud "application"
                QDomElement dom_node = domdoc.createElement("node");
                dom_node.setAttribute("address", boxName);
                dom_node.setAttribute("object", "Automation");
                dom_appli.appendChild(dom_node);

                /* **********************************************************************
                ** Contraintes souples
                ** **********************************************************************/

                int ev = j_cstr["EndEvent"].toObject() ["IdentifierValue"].toInt();

                if(min != max || !expression[ev].isEmpty())
                {
                    QDomElement TriggerNode = domdoc.createElement("node");
                    TriggerNode.setAttribute("address", "end");
                    TriggerNode.setAttribute("object", "TimeCondition");
                    dom_box.appendChild(TriggerNode);


                    QString trigId = "j";
                    trigId += QString::number(ev + 1);

                    // on regarde si le trigger point à déjà été crée (liste des conditions)
                    auto cond = dom_scenar.firstChildElement("condition");

                    while(!cond.isNull() && cond.attribute("name") != trigId)
                    {
                        cond = cond.nextSiblingElement("condition");
                    }

                    // si il n'y est pas, on l'ajoute
                    if(cond.isNull())
                    {
                        if(expression[ev].isNull())
                        {
                            expression[ev] = QString("/" + boxName + "/end");
                        }

                        QDomElement dom_condition = domdoc.createElement("condition");
                        dom_condition.setAttribute("name", trigId);
                        dom_condition.setAttribute("dispose", "");
                        dom_scenar.appendChild(dom_condition);

                        QDomElement dom_case = domdoc.createElement("case");
                        dom_case.setAttribute("event", boxEnd);
                        dom_case.setAttribute("trigger", expression[ev]);
                        dom_case.setAttribute("default", "1");
                        dom_condition.appendChild(dom_case);
                    }

                    // on regarde si le trigger point à déjà été crée (liste des events)
                    auto trig = dom_scenar.firstChildElement("event");

                    while(!trig.isNull() &&  trig.attribute("name") != boxEnd)
                    {
                        trig = trig.nextSiblingElement("event");
                    }

                    if(!trig.isNull())
                    {
                        trig.setAttribute("condition", trigId);
                    }
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////
    // Writing into file
    //////////////////////////////////////////////////////////////
    return domdoc.toString();
    /*
        QFile file("out.xml");
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return 1;
        }
        QTextStream out(&file);
        out << xml;
        file.close();
        */
}
