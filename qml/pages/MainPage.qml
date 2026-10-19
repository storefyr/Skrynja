import QtQuick 2.0
import Sailfish.Silica 1.0
import QtQuick.Layouts 1.1

Page {
    id: mainPage
    
    property string currentInput: ""
    property string currentResponse: ""
    property bool showSettings: false
    
    // Mørkt tema understøttelse for Sailfish OS 5
    property bool isDarkTheme: Theme.colorScheme === Theme.Dark
    
    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge
        
        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium
            
            // Header
            Label {
                text: "Skrynja"
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
                anchors.horizontalCenter: parent.horizontalCenter
                padding: Theme.paddingMedium
            }
            
            // Settings button
            Button {
                text: "Indstillinger"
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: showSettings = true
            }
            
            // Chat history
            ListView {
                id: chatList
                width: parent.width
                height: implicitHeight
                model: chatHistoryModel
                delegate: chatDelegate
                spacing: Theme.paddingSmall
                clip: true
                
                // Mørkt tema baggrund
                background: Rectangle {
                    color: isDarkTheme ? Theme.secondaryBackgroundColor : Theme.backgroundColor
                }
            }
            
            // Input area
            Row {
                width: parent.width
                spacing: Theme.paddingSmall
                padding: Theme.paddingSmall
                
                TextField {
                    id: inputField
                    width: parent.width * 0.8
                    placeholderText: "Skriv dit spørgsmål..."
                    text: currentInput
                    onTextChanged: currentInput = text
                    onAccepted: sendMessage()
                }
                
                Button {
                    text: "Send"
                    width: parent.width * 0.2
                    enabled: currentInput.length > 0 && !mistralClient.isLoading
                    onClicked: sendMessage()
                }
            }
            
            // Loading indicator
            BusyIndicator {
                id: loadingIndicator
                anchors.horizontalCenter: parent.horizontalCenter
                visible: mistralClient.isLoading
                running: mistralClient.isLoading
            }
            
            // Error message
            Label {
                id: errorLabel
                text: mistralClient.lastError
                color: Theme.errorColor
                visible: mistralClient.lastError.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
    
    // Settings dialog med mørkt tema understøttelse
    Dialog {
        id: settingsDialog
        title: "Indstillinger"
        anchors.fill: parent
        visible: showSettings
        standardButtons: Dialog.Ok | Dialog.Cancel
        
        // Mørkt tema for dialog
        background: Rectangle {
            color: isDarkTheme ? Theme.secondaryBackgroundColor : Theme.backgroundColor
        }
        
        onAccepted: {
            showSettings = false
        }
        onRejected: showSettings = false
        
        Column {
            width: parent.width
            spacing: Theme.paddingMedium
            
            Label {
                text: "API Nøgle (skal starte med 'mistral-')"
                font.pixelSize: Theme.fontSizeSmall
            }
            
            TextField {
                id: apiKeyField
                width: parent.width
                placeholderText: "Indtast din Mistral AI API nøgle"
                text: mistralClient.apiKey
                echoMode: TextInput.Password
                onAccepted: mistralClient.apiKey = text
            }
            
            Label {
                text: "Model"
                font.pixelSize: Theme.fontSizeSmall
            }
            
            ComboBox {
                id: modelCombo
                width: parent.width
                model: mistralClient.availableModels
                currentIndex: {
                    var idx = modelCombo.find(mistralClient.model);
                    return idx >= 0 ? idx : 0;
                }
                onCurrentIndexChanged: mistralClient.model = modelCombo.currentText
            }
        }
    }
    
    ListModel {
        id: chatHistoryModel
    }
    
    Component {
        id: chatDelegate
        Row {
            width: parent.width
            spacing: Theme.paddingSmall
            padding: Theme.paddingSmall
            
            Rectangle {
                width: childrenRect.width + Theme.paddingMedium * 2
                height: childrenRect.height + Theme.paddingMedium * 2
                color: role === "user" ? 
                       (isDarkTheme ? "#3a7bc8" : Theme.highlightColor) :
                       (isDarkTheme ? "#2a5a98" : Theme.secondaryHighlightColor)
                radius: Theme.radiusSmall
                opacity: isDarkTheme ? 0.9 : 1.0
                
                Column {
                    spacing: Theme.paddingSmall
                    
                    Label {
                        text: role === "user" ? "Dig:" : "Skrynja:"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.primaryColor
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    
                    Label {
                        text: content
                        font.pixelSize: Theme.fontSizeMedium
                        color: isDarkTheme ? Qt.lighter(Theme.primaryColor, 1.5) : Theme.primaryColor
                        width: parent.width * 0.9
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
    
    function sendMessage() {
        if (currentInput.length === 0 || mistralClient.isLoading) {
            return
        }
        
        var messageText = currentInput
        currentInput = ""
        inputField.text = ""
        
        // Add user message to history
        chatHistoryModel.append({"role": "user", "content": messageText})
        
        // Build complete history with both user and assistant messages
        var history = []
        for (var i = 0; i < chatHistoryModel.count; i++) {
            var item = chatHistoryModel.get(i)
            history.push({"role": item.role, "content": item.content})
        }
        
        mistralClient.sendMessage(history)
    }
    
    Connections {
        target: mistralClient
        function onMessageReceived(response) {
            chatHistoryModel.append({"role": "assistant", "content": response})
            chatList.positionViewAtEnd()
        }
        

    }
    

}
