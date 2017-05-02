import QtQuick 2.5
import CreativeControls 1.0
import Ossia 1.0 as Ossia
import QtMultimedia 5.5
Item {
    id: root

    Video
    {
        loops: MediaPlayer.Infinite
        anchors.fill: parent
        autoPlay: true
        muted: true
        source: "/home/jcelerier/travail/videoplayer/vide_silent.mp4"
        Ossia.Property on opacity {
            device: Ossia.SingleDevice
            node: '/video1/opacity'
        }
    }

    Video
    {
        y: 400
        loops: MediaPlayer.Infinite
        width: 200
        height: 300
        autoPlay: true
        muted: true
        source: "/home/jcelerier/Vidéos/heyaheya.mkv"
        Ossia.Property on opacity {
            device: Ossia.SingleDevice
            node: '/video2/opacity'
        }
        Ossia.Property on rotation {
            device: Ossia.SingleDevice
            node: '/video2/rotation'
        }
        Ossia.Property on width {
            device: Ossia.SingleDevice
            node: '/video2/width'
        }
        Ossia.Property on width {
            device: Ossia.SingleDevice
            node: '/video2/height'
        }

    }

    Joystick {
        width: 200
        height: 200
        Ossia.Property on stickR {
            device: Ossia.SingleDevice
            node: '/stick'
        }
    }


    Ossia.Player {
        id: p
    }
    Component.onCompleted: {
        Ossia.SingleDevice.setName("OSCdevice");
        Ossia.SingleDevice.recreate(root)

//        p.load("/home/jcelerier/travail/videoplayer/ex.scorejson")
//        p.play()
    }
}
