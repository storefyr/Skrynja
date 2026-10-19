import QtQuick 2.0
import Sailfish.Silica 1.0

Dialog {
    id: settingsDialog

    Column {
        width: parent.width

        DialogHeader {
            title: "Indstillinger"
            acceptText: "OK"
            cancelText: "Annuller"
        }

        PasswordField {
            id: apiKeyField
            width: parent.width
            label: "Mistral API-nøgle"
            placeholderText: "mistral-…"
            text: mistralClient.apiKey
        }

        ComboBox {
            id: modelCombo
            width: parent.width
            label: "Model"
            menu: ContextMenu {
                Repeater {
                    model: mistralClient.availableModels
                    MenuItem {
                        text: modelData
                    }
                }
            }
            property bool _ready: false
            Component.onCompleted: {
                var models = mistralClient.availableModels
                for (var i = 0; i < models.length; i++) {
                    if (models[i] === mistralClient.model) {
                        currentIndex = i
                        break
                    }
                }
                _ready = true
            }
            onCurrentIndexChanged: {
                if (_ready && currentIndex >= 0) {
                    mistralClient.model = mistralClient.availableModels[currentIndex]
                }
            }
        }
    }

    onAccepted: {
        mistralClient.apiKey = apiKeyField.text
    }
}
