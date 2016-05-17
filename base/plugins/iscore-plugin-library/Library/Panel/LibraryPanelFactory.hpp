/*
static const iscore::DefaultPanelStatus status{false, Qt::RightDockWidgetArea, 0, QObject::tr("Library")};

const QString shortcut() const override
{ return tr("Ctrl+L"); }
LibraryPanelView::LibraryPanelView(QObject* parent):
    iscore::PanelView {parent},
    m_widget{new QTabWidget}
{
    auto projectLib = new LibraryWidget{m_widget};
    m_widget->addTab(projectLib, tr("Project"));

    auto systemLib = new LibraryWidget{m_widget};
    m_widget->addTab(systemLib, tr("System"));

    m_widget->setObjectName("LibraryExplorer");
}
*/
