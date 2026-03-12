/*
 * NEXUS OS - Boot Manager GUI
 * Copyright (c) 2024 NEXUS Development Team
 *
 * Main.qml - Boot Manager User Interface
 * 
 * Modern, Material Design-inspired UI for boot configuration
 * and virtualization mode selection
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 1024
    height: 720
    minimumWidth: 800
    minimumHeight: 600
    title: qsTr("NEXUS OS Boot Manager - ") + appVersion
    
    // Material Design Theme
    Material.theme: Material.Dark
    Material.primary: Material.Blue
    Material.accent: Material.Cyan
    
    // Window icon
    // icon.source: "qrc:/icons/nexus-logo.png"
    
    // Header
    header: ToolBar {
        Material.elevation: 4
        z: 10
        
        RowLayout {
            anchors.fill: parent
            spacing: 20
            
            // Logo and Title
            RowLayout {
                spacing: 12
                
                Image {
                    source: "qrc:/icons/nexus-logo.svg"
                    width: 36
                    height: 36
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
                
                ColumnLayout {
                    Layout.leftMargin: 8
                    
                    Label {
                        text: "NEXUS OS"
                        font.pixelSize: 18
                        font.bold: true
                        color: Material.color(Material.Blue, Material.Shade100)
                    }
                    
                    Label {
                        text: "Boot Manager"
                        font.pixelSize: 11
                        color: Material.color(Material.Grey, Material.Shade300)
                    }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            // Action Buttons
            RowLayout {
                spacing: 8
                
                ToolButton {
                    text: "Reload"
                    icon.source: "qrc:/icons/reload.svg"
                    onClicked: bootConfig.reloadConfiguration()
                    ToolTip.text: "Reload configuration"
                    ToolTip.visible: hovered
                }
                
                ToolButton {
                    text: "Add Entry"
                    icon.source: "qrc:/icons/add.svg"
                    onClicked: entryDialog.open()
                    ToolTip.text: "Add boot entry"
                    ToolTip.visible: hovered
                }
                
                ToolButton {
                    text: "Save"
                    icon.source: "qrc:/icons/save.svg"
                    enabled: bootConfig.isModified
                    onClicked: {
                        if (bootConfig.saveConfiguration()) {
                            statusMessage.show("Configuration saved", Message.Success)
                        } else {
                            statusMessage.show("Failed to save configuration", Message.Error)
                        }
                    }
                    ToolTip.text: "Save changes"
                    ToolTip.visible: hovered
                }
                
                MenuSeparator {}
                
                ToolButton {
                    text: "Help"
                    icon.source: "qrc:/icons/help.svg"
                    onClicked: helpDialog.open()
                }
            }
        }
    }
    
    // Main Content
    StackLayout {
        anchors.fill: parent
        currentIndex: tabBar.currentIndex
        
        // Tab 1: Boot Entries
        Item {
            id: bootEntriesPage
            
            RowLayout {
                anchors.fill: parent
                spacing: 0
                
                // Boot Entries List
                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 16
                    Material.elevation: 2
                    
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0
                        
                        // List Header
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            color: Material.color(Material.Blue, Material.Shade900)
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                
                                Label {
                                    text: "Boot Entries"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: Material.color(Material.Blue, Material.Shade100)
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                
                                Item { Layout.fillWidth: true }
                                
                                Label {
                                    text: bootConfig.count + " entries"
                                    font.pixelSize: 12
                                    color: Material.color(Material.Blue, Material.Shade300)
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }
                        }
                        
                        // Entries List
                        ListView {
                            id: entriesListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: bootConfig
                            currentIndex: -1
                            
                            delegate: ItemDelegate {
                                id: entryDelegate
                                width: entriesListView.width
                                height: 80
                                highlighted: ListView.isCurrentItem
                                onClicked: entriesListView.currentIndex = index
                                
                                contentItem: RowLayout {
                                    spacing: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    
                                    // Status Indicator
                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 6
                                        color: model.isEnabled ? 
                                                   (model.isDefault ? Material.color(Material.Green) : Material.color(Material.Blue)) : 
                                                   Material.color(Material.Grey)
                                        visible: true
                                        
                                        ToolTip.text: model.isDefault ? "Default entry" : (model.isEnabled ? "Enabled" : "Disabled")
                                        ToolTip.visible: entryDelegate.hovered
                                    }
                                    
                                    // Entry Info
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        
                                        Label {
                                            text: model.name
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: Material.color(Material.Blue, Material.Shade100)
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        
                                        Label {
                                            text: model.kernelPath
                                            font.pixelSize: 11
                                            color: Material.color(Material.Grey, Material.Shade500)
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                        
                                        RowLayout {
                                            spacing: 12
                                            
                                            Label {
                                                text: "⏱ " + (model.timeout > 0 ? model.timeout + "s" : "—")
                                                font.pixelSize: 10
                                                color: Material.color(Material.Grey, Material.Shade600)
                                            }
                                            
                                            Label {
                                                text: "🔄 " + model.virtMode
                                                font.pixelSize: 10
                                                color: Material.color(Material.Grey, Material.Shade600)
                                            }
                                            
                                            Label {
                                                text: "👢 " + model.bootCount
                                                font.pixelSize: 10
                                                color: Material.color(Material.Grey, Material.Shade600)
                                            }
                                            
                                            Item { Layout.fillWidth: true }
                                            
                                            Label {
                                                text: model.lastBooted ? "Last: " + model.lastBooted : "Never booted"
                                                font.pixelSize: 10
                                                color: Material.color(Material.Grey, Material.Shade700)
                                            }
                                        }
                                    }
                                    
                                    // Actions
                                    RowLayout {
                                        spacing: 4
                                        visible: entryDelegate.hovered
                                        
                                        ToolButton {
                                            icon.source: "qrc:/icons/edit.svg"
                                            icon.color: Material.color(Material.Blue)
                                            onClicked: {
                                                editEntry(index)
                                            }
                                            ToolTip.text: "Edit entry"
                                        }
                                        
                                        ToolButton {
                                            icon.source: model.isDefault ? "qrc:/icons/star-filled.svg" : "qrc:/icons/star.svg"
                                            icon.color: Material.color(Material.Yellow)
                                            onClicked: bootConfig.setDefaultEntry(index)
                                            ToolTip.text: "Set as default"
                                        }
                                        
                                        ToolButton {
                                            icon.source: model.isEnabled ? "qrc:/icons/visible.svg" : "qrc:/icons/hidden.svg"
                                            icon.color: Material.color(Material.Green)
                                            onClicked: bootConfig.setEntryEnabled(index, !model.isEnabled)
                                            ToolTip.text: model.isEnabled ? "Disable" : "Enable"
                                        }
                                        
                                        ToolButton {
                                            icon.source: "qrc:/icons/delete.svg"
                                            icon.color: Material.color(Material.Red)
                                            enabled: bootConfig.count > 1
                                            onClicked: deleteConfirmationDialog.index = index, deleteConfirmationDialog.open()
                                            ToolTip.text: "Delete entry"
                                        }
                                    }
                                }
                            }
                            
                            // Empty state
                            Label {
                                anchors.centerIn: parent
                                text: "No boot entries\nClick 'Add Entry' to create one"
                                font.pixelSize: 14
                                color: Material.color(Material.Grey, Material.Shade500)
                                horizontalAlignment: Text.AlignHCenter
                                visible: bootConfig.count === 0
                            }
                            
                            // Scrollbar
                            ScrollBar.vertical: ScrollBar {
                                active: true
                            }
                        }
                    }
                }
                
                // Right Panel - Virtualization Mode Selection
                Card {
                    Layout.preferredWidth: 380
                    Layout.fillHeight: true
                    Layout.margins: 16
                    Material.elevation: 2
                    
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0
                        
                        // Header
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            color: Material.color(Material.Cyan, Material.Shade900)
                            
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 4
                                
                                Label {
                                    text: "Virtualization Mode"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: Material.color(Material.Cyan, Material.Shade100)
                                }
                                
                                Label {
                                    text: "Select boot-time virtualization"
                                    font.pixelSize: 11
                                    color: Material.color(Material.Cyan, Material.Shade300)
                                }
                            }
                        }
                        
                        // Mode Selection List
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: virtModeManager
                            
                            delegate: ItemDelegate {
                                id: modeDelegate
                                width: modesListView.width
                                height: 100
                                highlighted: virtModeManager.selectedMode === model.modeId
                                onClicked: virtModeManager.selectedMode = model.modeId
                                
                                background: Rectangle {
                                    color: modeDelegate.highlighted ? 
                                              Material.color(Material.Cyan, Material.Shade900) : 
                                              (modeDelegate.hovered ? Material.color(Material.Grey, Material.Shade900) : "transparent")
                                    border.color: modeDelegate.highlighted ? Material.color(Material.Cyan) : "transparent"
                                    border.width: 2
                                }
                                
                                contentItem: RowLayout {
                                    spacing: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    
                                    // Icon placeholder
                                    Rectangle {
                                        width: 40
                                        height: 40
                                        radius: 20
                                        color: model.isAvailable ? 
                                                  Material.color(Material.Cyan, model.securityLevel >= 4 ? Material.Shade700 : Material.Shade800) : 
                                                  Material.color(Material.Grey, Material.Shade800)
                                        
                                        Image {
                                            anchors.centerIn: parent
                                            source: model.modeIcon
                                            width: 24
                                            height: 24
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            visible: false // Fallback if icon not found
                                        }
                                        
                                        Label {
                                            anchors.centerIn: parent
                                            text: model.securityLevel >= 4 ? "🔒" : "⚡"
                                            font.pixelSize: 20
                                        }
                                    }
                                    
                                    // Mode Info
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        
                                        RowLayout {
                                            Layout.fillWidth: true
                                            
                                            Label {
                                                text: model.modeName
                                                font.pixelSize: 13
                                                font.bold: true
                                                color: model.isAvailable ? 
                                                          Material.color(Material.Cyan, Material.Shade100) : 
                                                          Material.color(Material.Grey, Material.Shade500)
                                                Layout.fillWidth: true
                                            }
                                            
                                            // Availability indicator
                                            Rectangle {
                                                width: 8
                                                height: 8
                                                radius: 4
                                                color: model.isAvailable ? Material.color(Material.Green) : Material.color(Material.Red)
                                                visible: true
                                                
                                                ToolTip.text: model.isAvailable ? "Available" : "Not available"
                                            }
                                        }
                                        
                                        Label {
                                            text: model.modeDescription
                                            font.pixelSize: 10
                                            color: Material.color(Material.Grey, Material.Shade400)
                                            wrapMode: Text.WordWrap
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        
                                        // Security and Performance indicators
                                        RowLayout {
                                            spacing: 8
                                            
                                            Label {
                                                text: "🛡️ Security: " + model.securityLevel + "/5"
                                                font.pixelSize: 9
                                                color: Material.color(Material.Grey, Material.Shade500)
                                            }
                                            
                                            Label {
                                                text: "⚡ Performance: " + model.performanceLevel + "/5"
                                                font.pixelSize: 9
                                                color: Material.color(Material.Grey, Material.Shade500)
                                            }
                                        }
                                    }
                                }
                            }
                            
                            ScrollBar.vertical: ScrollBar {
                                active: true
                            }
                        }
                        
                        // System Info Footer
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            color: Material.color(Material.Grey, Material.Shade900)
                            
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 4
                                
                                Label {
                                    text: "System Information"
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: Material.color(Material.Grey, Material.Shade300)
                                }
                                
                                Label {
                                    text: virtModeManager.systemInfo
                                    font.pixelSize: 9
                                    color: Material.color(Material.Grey, Material.Shade500)
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Tab 2: System Settings (placeholder)
        Item {
            id: settingsPage
            
            Label {
                anchors.centerIn: parent
                text: "System Settings\nComing soon..."
                font.pixelSize: 18
                color: Material.color(Material.Grey, Material.Shade500)
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
    
    // Bottom Tab Bar
    footer: TabBar {
        id: tabBar
        currentIndex: 0
        
        TabButton {
            text: "Boot Entries"
            icon.source: "qrc:/icons/boot.svg"
        }
        
        TabButton {
            text: "System Settings"
            icon.source: "qrc:/icons/settings.svg"
            enabled: false
        }
    }
    
    // Status Message Component
    Message {
        id: statusMessage
    }
    
    // Add/Edit Entry Dialog
    EntryDialog {
        id: entryDialog
        onEntryAccepted: function(name, kernelPath, initrdPath, cmdline, virtMode) {
            bootConfig.addEntry(name, kernelPath, initrdPath, cmdline, virtMode)
            statusMessage.show("Entry added successfully", Message.Success)
        }
    }
    
    // Delete Confirmation Dialog
    Dialog {
        id: deleteConfirmationDialog
        parent: mainWindow.overlay
        modal: true
        anchors.centerIn: parent
        width: 400
        
        property int index: -1
        
        title: "Confirm Deletion"
        
        Label {
            text: "Are you sure you want to delete this boot entry?\n\nThis action cannot be undone."
            wrapMode: Text.WordWrap
            width: parent.width
        }
        
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            if (deleteConfirmationDialog.index >= 0) {
                bootConfig.removeEntry(deleteConfirmationDialog.index)
                statusMessage.show("Entry deleted", Message.Info)
            }
        }
    }
    
    // Help Dialog
    Dialog {
        id: helpDialog
        parent: mainWindow.overlay
        modal: true
        anchors.centerIn: parent
        width: 600
        height: 500
        
        title: "NEXUS OS Boot Manager Help"
        
        ScrollView {
            anchors.fill: parent
            clip: true
            
            Label {
                width: parent.width - 32
                wrapMode: Text.WordWrap
                text: `
<h2>NEXUS OS Boot Manager</h2>

<h3>Getting Started</h3>
<p>The Boot Manager allows you to configure boot entries and select virtualization modes for your NEXUS OS system.</p>

<h3>Boot Entries</h3>
<p>Each boot entry represents a different way to start your system:</p>
<ul>
<li><b>Name:</b> Display name for the entry</li>
<li><b>Kernel Path:</b> Location of the kernel file</li>
<li><b>Initrd Path:</b> Initial ramdisk (optional)</li>
<li><b>Command Line:</b> Kernel parameters</li>
<li><b>Virtualization Mode:</b> How the system virtualizes</li>
</ul>

<h3>Virtualization Modes</h3>
<ul>
<li><b>Native:</b> Direct hardware, maximum performance</li>
<li><b>Light:</b> Process isolation, balanced (Recommended)</li>
<li><b>Full:</b> Complete virtualization, enterprise security</li>
<li><b>Container:</b> Lightweight, development optimized</li>
<li><b>Secure Enclave:</b> Maximum security isolation</li>
<li><b>Compatibility:</b> Legacy OS support</li>
<li><b>Custom:</b> Advanced configuration</li>
</ul>

<h3>Tips</h3>
<ul>
<li>Set a default entry for automatic boot</li>
<li>Use Safe Mode if you encounter issues</li>
<li>Debug Mode provides detailed logging</li>
<li>Memory Test checks your RAM</li>
</ul>

<p><i>Version: </i>` + appVersion + `</p>
`
            }
        }
        
        standardButtons: Dialog.Close
    }
    
    // Functions
    function editEntry(index) {
        // Open edit dialog with existing entry data
        entryDialog.entryIndex = index
        entryDialog.open()
    }
}
