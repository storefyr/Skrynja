import QtQuick 2.0
import Sailfish.Silica 1.0

CoverPage {
    id: coverPage
    
    // Mørkt tema understøttelse
    property bool isDarkTheme: Theme.colorScheme === Theme.Dark
    
    Label {
        text: "Skrynja"
        anchors.centerIn: parent
        font.pixelSize: Theme.fontSizeLarge
        color: Theme.highlightColor
    }
    
    // Baggrund for cover page
    background: Rectangle {
        color: isDarkTheme ? Theme.secondaryBackgroundColor : Theme.backgroundColor
    }
}
