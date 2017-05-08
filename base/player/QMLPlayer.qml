import QtQuick 2.5
import Ossia 1.0 as Ossia
import QtQuick.Controls 2.1
import QtMultimedia 5.5
Item {
    id: root

    Video
    {
        onStopped: play()
        //loops: MediaPlayer.Infinite
        anchors.fill: parent
        autoPlay: true
        muted: true
        source: "/home/jcelerier/video/vid1.mp4"
        Ossia.Property on opacity {
            device: Ossia.SingleDevice
            node: '/video1/opacity'
        }
    }

    Video
    {
        onStopped: play()

        //loops: MediaPlayer.Infinite
        height: 200
        width: 200
        x: 0.5 * parent.width - width / 2
        y: 0.5 * parent.height - height / 2
        autoPlay: true
        muted: true
        source: "/home/jcelerier/video/vid2.mp4"
        Ossia.Property on opacity {
            device: Ossia.SingleDevice
            node: '/video2/opacity'
        }
        Ossia.Property on rotation {
            device: Ossia.SingleDevice
            node: '/video2/rotation'
        }
        Ossia.Property on scale {
            device: Ossia.SingleDevice
            node: '/video2/width'
        }/*
        Ossia.Property on height {
            device: Ossia.SingleDevice
            node: '/video2/height'
        }*/

    }

    Row {
    Slider {
        id: sl
        width: 200
        property real vpos : visualPosition
        Ossia.Property on vpos {
            device: Ossia.SingleDevice
            node: '/stick'
        }
    }

    Button {
        property bool t: false
        text: "Trigger"
        onClicked: t = !t;
        Ossia.Property on t {
            device: Ossia.SingleDevice
            node: '/push'
        }
    }
    }

    Ossia.Player {
        id: p
        port: 5567
    }
    Component.onCompleted: {
        Ossia.SingleDevice.setName("OSCdevice");
        Ossia.SingleDevice.recreate(root)

//        p.load("/home/jcelerier/travail/videoplayer/ex.scorejson")
//        p.play()
    }
}
