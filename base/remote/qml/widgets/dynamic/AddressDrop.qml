import QtQuick 2.11
QtObject
{
    property var item

    function startDragging(drag)
    {
        item.color = "#FCC"
    }

    function stopDragging(drag)
    {
        item.color = "#00000000"
    }

    function dragging(drag)
    {

    }

    function dropping(drop)
    {
        stopDragging(drop)

        var drop_fmt = drop.formats[0];
        var drop_text = drop.getDataAsString(drop_fmt);
        if(drop_fmt === "iscore/x-remote-address")
        {
            item.addressChanged(drop_text);
            return true;
        }
    }
}
