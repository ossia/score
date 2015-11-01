#include "AddProcessDialog.hpp"

#include <Process/ProcessList.hpp>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QApplication>
AddProcessDialog::AddProcessDialog(QWidget *parent) :
    QWidget {parent}
{
    hide();
}

void AddProcessDialog::launchWindow()
{
    bool ok = false;
    auto process_list = ProcessList::getProcessesName();
    process_list.sort();
    auto process_name = QInputDialog::getItem(qApp->activeWindow(),
        tr("Choose a process"),
        tr("Choose a process"),
        process_list,
        0,
        false,
        &ok);

    if(ok)
    {
        emit okPressed(process_name);
    }
}
