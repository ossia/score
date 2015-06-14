#include "AddProcessDialog.hpp"

#include "ProcessInterface/ProcessList.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>

AddProcessDialog::AddProcessDialog(QWidget *parent) :
    QWidget {parent}
{
    hide();
}

void AddProcessDialog::launchWindow()
{
    bool ok = false;
    auto process_list = ProcessList::getProcessesName();
    auto process_name = QInputDialog::getItem(this,
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
