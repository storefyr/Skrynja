import QtQuick 2.0
import Sailfish.Silica 1.0
import QtQuick.Controls 1.0

ApplicationWindow {
    id: appWindow
    initialPage: Component { MainPage {} }
    cover: Component { CoverPage {} }
    allowedOrientations: defaultAllowedOrientations
}
