import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "../components/Categories.js" as Categories

// Per-conversation overrides: title, category and system prompt.
// The model is fixed at creation and cannot be changed here.
//
// This is a Page, not a Dialog: it is attached to the chat (one swipe forward)
// and follows the current conversation. Changes are written when leaving it,
// the way Silica settings pages behave.
Page {
    id: conversationSettings
    allowedOrientations: Orientation.All

    // Empty means "whatever conversation is currently open". The history
    // pushes this page with an explicit id instead.
    property string conversationId: ""

    // Conversation the fields were last loaded from, and the one changes are
    // written back to. Keeping it separate matters: the current conversation
    // can change while this page is alive.
    property string loadedId: ""
    property bool suggestingTitle: false

    // Values as loaded, so leaving the page without touching anything does not
    // rewrite the conversation file and bump its position in the history.
    property string loadedTitle: ""
    property string loadedPrompt: ""
    property string loadedCategory: ""
    property bool loadedDevTools: false

    property var categoryOptions: Categories.all()

    onStatusChanged: {
        if (status === PageStatus.Activating) {
            reload()
        } else if (status === PageStatus.Deactivating) {
            apply()
        }
    }

    Component.onCompleted: reload()
    Component.onDestruction: apply()

    Connections {
        target: conversationManager
        onCurrentConversationChanged: {
            if (conversationSettings.conversationId !== "") {
                return
            }
            // Write to the conversation the fields belong to before adopting
            // the new one.
            conversationSettings.apply()
            conversationSettings.loadedId = ""
            conversationSettings.reload()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Conversation settings")
            }

            TextField {
                id: titleField
                width: parent.width
                label: qsTr("Title")
                placeholderText: qsTr("Conversation title")

                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: conversationSettings.suggestingTitle
                      ? qsTr("Asking the model...")
                      : qsTr("Suggest a title")
                enabled: !conversationSettings.suggestingTitle && settingsManager.hasApiKey
                onClicked: conversationSettings.suggestTitle()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Reads the conversation and proposes a title and a category.")
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: qsTr("Category")
            }

            ComboBox {
                id: categoryCombo
                width: parent.width
                label: qsTr("Topic")
                description: qsTr("Used by the statistics page")

                menu: ContextMenu {
                    Repeater {
                        model: conversationSettings.categoryOptions

                        MenuItem {
                            text: Categories.label(modelData)
                            property string categoryValue: modelData
                        }
                    }
                }
            }

            // The model is fixed at creation (Conversations API locks it
            // server-side), so it is shown, not chosen.
            SectionHeader {
                text: qsTr("Model")
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: conversationManager.conversationModel(loadedId) !== ""
                      ? conversationManager.conversationModel(loadedId)
                      : qsTr("Default (%1)").arg(settingsManager.modelName)
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Fixed when the conversation was created")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: qsTr("System prompt")
            }

            TextSwitch {
                id: usePromptSwitch
                text: qsTr("Custom system prompt")
                description: qsTr("Replaces the global system prompt for this conversation only")
            }

            TextArea {
                id: promptArea
                width: parent.width
                visible: usePromptSwitch.checked
                label: qsTr("Instruction")
                placeholderText: qsTr("Enter a system prompt...")
            }

            SectionHeader {
                text: qsTr("Developer mode")
            }

            TextSwitch {
                id: devToolsSwitch
                text: qsTr("Developer mode")
                description: qsTr("The assistant can read, edit and create files and run commands in a local workspace on this device")
                checked: false
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Workspace: %1. Files are cloned from GitHub the first time it is used.").arg(devTools.workspaceRoot())
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.WordWrap
                visible: devToolsSwitch.checked
            }

            Item {
                width: parent.width
                height: Theme.paddingLarge
            }
        }

        VerticalScrollDecorator {}
    }

    Connections {
        target: mistralApi

        onTitleGenerated: {
            if (targetId !== conversationSettings.loadedId) {
                return
            }
            conversationSettings.suggestingTitle = false
            titleField.text = title

            var index = conversationSettings.categoryOptions.indexOf(category)
            if (index >= 0) {
                categoryCombo.currentIndex = index
            }
        }

        onTitleGenerationFailed: {
            if (targetId === conversationSettings.loadedId) {
                conversationSettings.suggestingTitle = false
            }
        }
    }

    function reload() {
        var id = conversationId !== "" ? conversationId
                                       : conversationManager.currentConversationId()
        if (id === "" || id === loadedId) {
            return
        }
        loadedId = id

        var overrides = conversationManager.getConversationOverrides(id)

        loadedTitle = overrides.title || ""
        loadedPrompt = overrides.systemPrompt || ""
        loadedCategory = overrides.category || "other"

        titleField.text = loadedTitle
        usePromptSwitch.checked = loadedPrompt !== ""
        promptArea.text = loadedPrompt

        var categoryIndex = categoryOptions.indexOf(loadedCategory)
        categoryCombo.currentIndex = categoryIndex >= 0 ? categoryIndex
                                                        : categoryOptions.length - 1

        loadedDevTools = conversationManager.isDevToolsEnabled(loadedId)
        devToolsSwitch.checked = loadedDevTools
    }

    function apply() {
        if (loadedId === "") {
            return
        }

        var title = titleField.text.trim()
        var prompt = usePromptSwitch.checked ? promptArea.text.trim() : ""
        var category = categoryCombo.currentItem ? categoryCombo.currentItem.categoryValue : ""

        if (title !== loadedTitle) {
            conversationManager.renameConversation(loadedId, title)
            loadedTitle = title
        }

        if (prompt !== loadedPrompt) {
            conversationManager.setConversationOverrides(loadedId, prompt)
            loadedPrompt = prompt
        }

        if (category !== "" && category !== loadedCategory) {
            conversationManager.setConversationCategory(loadedId, category)
            loadedCategory = category
        }

        if (devToolsSwitch.checked !== loadedDevTools) {
            conversationManager.setDevToolsEnabled(loadedId, devToolsSwitch.checked)
            loadedDevTools = devToolsSwitch.checked
        }
    }

    function suggestTitle() {
        var digest = conversationManager.conversationDigest(loadedId)
        if (digest === "") {
            return
        }

        suggestingTitle = true
        // Use the model this conversation was created with.
        var modelId = conversationManager.conversationModel(loadedId)
        if (modelId === "") {
            modelId = settingsManager.modelName
        }
        mistralApi.generateTitle(
                    settingsManager.apiKey,
                    settingsManager.resolveModel(modelId),
                    digest, loadedId)
    }
}
