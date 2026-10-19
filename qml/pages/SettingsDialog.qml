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
            property string selectedModel: mistralClient.model
            menu: ContextMenu {
                Repeater {
                    model: mistralClient.availableModels
                    MenuItem {
                        text: modelData
                    }
                }
            }
            Component.onCompleted: {
                var models = mistralClient.availableModels
                for (var i = 0; i < models.length; i++) {
                    if (models[i] === selectedModel) {
                        currentIndex = i
                        break
                    }
                }
            }
            onCurrentIndexChanged: {
                if (currentIndex >= 0) {
                    selectedModel = mistralClient.availableModels[currentIndex]
                }
            }
        }
    }

    onAccepted: {
        mistralClient.apiKey = apiKeyField.text
        mistralClient.model = modelCombo.selectedModel
    }
}
