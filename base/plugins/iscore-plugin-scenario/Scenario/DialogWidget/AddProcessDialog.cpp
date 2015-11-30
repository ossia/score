#include <Process/ProcessList.hpp>
#include <QApplication>
#include <QInputDialog>

#include <QString>
#include <QStringList>
#include <algorithm>
#include <utility>
#include <vector>

#include "AddProcessDialog.hpp"
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>

AddProcessDialog::AddProcessDialog(
        const DynamicProcessList& plist,
        QWidget *parent) :
    QWidget {parent},
    m_factoryList{plist}
{
    hide();
}

void AddProcessDialog::launchWindow()
{
    bool ok = false;

    std::vector<std::pair<QString, ProcessFactoryKey>> sortedFactoryList;
    for(const auto& factory : m_factoryList.list().get())
    {
        sortedFactoryList.push_back(
                    std::make_pair(
                        factory.second->prettyName(),
                        factory.first));
    }

    std::sort(sortedFactoryList.begin(),
              sortedFactoryList.end(),
              [] (const auto& e1, const auto& e2) {
        return e1.first < e2.first;
    });

    QStringList nameList;
    for(const auto& elt : sortedFactoryList)
    {
        nameList.append(elt.first);
    }

    auto process_name = QInputDialog::getItem(
        qApp->activeWindow(),
        tr("Choose a process"),
        tr("Choose a process"),
        nameList,
        0,
        false,
        &ok);


    if(ok)
    {
        auto it = std::find_if(sortedFactoryList.begin(),
                               sortedFactoryList.end(),
                               [&] (const auto& elt) {
            return elt.first == process_name;
        });
        ISCORE_ASSERT(it != sortedFactoryList.end());
        emit okPressed(it->second);
    }
}
