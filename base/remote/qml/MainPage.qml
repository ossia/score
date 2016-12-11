import QtQuick 2.7
import QtQuick.Layouts 1.3
MainPageForm {

    RowLayout
    {
        id: lay
        anchors.fill: parent

        NodeTree {
            Layout.preferredWidth: 100;
            implicitHeight: parent.height
            implicitWidth:100
            width: 100
        }

        WidgetList { Layout.preferredWidth: 200; implicitHeight: parent.height}

        CentralArea { Layout.preferredWidth: 200; implicitHeight: 200}

    }

}
