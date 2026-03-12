/*
 * NEXUS OS - Boot Manager GUI
 * Copyright (c) 2024 NEXUS Development Team
 *
 * EntryDialog.qml - Add/Edit Boot Entry Dialog
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: entryDialog
    parent: mainWindow ? mainWindow.overlay : null
    modal: true
    anchors.centerIn: parent
    width: 600
    height: 650
    
    property int entryIndex: -1
    property bool isEdit: entryIndex >= 0
    
    title: isEdit ? "Edit Boot Entry" : "Add Boot Entry"
    standardButtons: Dialog.Ok | Dialog.Cancel
    
    signal entryAccepted(string name, string kernelPath, string initrdPath, 
                         string cmdline, string virtMode)
    
    onOpened: {
        if (isEdit && bootConfig.count > entryIndex) {
            // Load existing entry data
            let entry = bootConfig.getEntry(entryIndex)
            nameField.text = entry.name
            kernelField.text = entry.kernelPath
            initrdField.text = entry.initrdPath
            cmdlineField.text = entry.cmdline
            virtModeCombo.currentIndex = virtModeCombo.indexOfValue(entry.virtMode)
        } else {
            // Reset fields for new entry
            nameField.text = ""
            kernelField.text = ""
            initrdField.text = ""
            cmdlineField.text = ""
            virtModeCombo.currentIndex = 1 // Light virtualization (default)
        }
    }
    
    onAccepted: {
        if (validateInputs()) {
            entryAccepted(
                nameField.text.trim(),
                kernelField.text.trim(),
                initrdField.text.trim(),
                cmdlineField.text.trim(),
                virtModeCombo.currentValue
            )
        } else {
            reject()
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 16
        
        // Name Field
        GroupBox {
            Layout.fillWidth: true
            title: "Entry Name"
            
            TextField {
                id: nameField
                width: parent.width - 24
                placeholderText: "e.g., NEXUS OS, Safe Mode, Debug Mode"
                text: ""
                
                Label {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: -20
                    font.pixelSize: 10
                    color: Material.color(Material.Grey, Material.Shade500)
                    text: "A descriptive name for this boot entry"
                }
            }
        }
        
        // Kernel Path Field
        GroupBox {
            Layout.fillWidth: true
            title: "Kernel Path"
            
            RowLayout {
                anchors.fill: parent
                spacing: 8
                
                TextField {
                    id: kernelField
                    Layout.fillWidth: true
                    placeholderText: "/boot/nexus-kernel"
                    text: ""
                    
                    onTextChanged: {
                        if (text.length > 0) {
                            initrdField.text = findInitrd(text)
                        }
                    }
                }
                
                Button {
                    text: "Browse"
                    onClicked: kernelFileDialog.open()
                }
            }
            
            Label {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -20
                font.pixelSize: 10
                color: Material.color(Material.Grey, Material.Shade500)
                text: "Path to the kernel image file"
            }
        }
        
        // Initrd Path Field
        GroupBox {
            Layout.fillWidth: true
            title: "Initrd Path (Optional)"
            
            RowLayout {
                anchors.fill: parent
                spacing: 8
                
                TextField {
                    id: initrdField
                    Layout.fillWidth: true
                    placeholderText: "/boot/nexus-initrd.img"
                    text: ""
                }
                
                Button {
                    text: "Browse"
                    onClicked: initrdFileDialog.open()
                }
            }
            
            Label {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -20
                font.pixelSize: 10
                color: Material.color(Material.Grey, Material.Shade500)
                text: "Initial ramdisk image (auto-detected if empty)"
            }
        }
        
        // Command Line Field
        GroupBox {
            Layout.fillWidth: true
            title: "Kernel Command Line"
            
            TextArea {
                id: cmdlineField
                width: parent.width - 24
                height: 80
                placeholderText: "quiet splash loglevel=3"
                text: ""
                wrapMode: TextArea.Wrap
            }
            
            RowLayout {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -20
                spacing: 8
                
                Label {
                    font.pixelSize: 10
                    color: Material.color(Material.Grey, Material.Shade500)
                    text: "Common options:"
                }
                
                Chip {
                    text: "quiet"
                    onClicked: appendCmdline("quiet")
                }
                
                Chip {
                    text: "splash"
                    onClicked: appendCmdline("splash")
                }
                
                Chip {
                    text: "debug"
                    onClicked: appendCmdline("debug")
                }
                
                Chip {
                    text: "nomodeset"
                    onClicked: appendCmdline("nomodeset")
                }
            }
        }
        
        // Virtualization Mode
        GroupBox {
            Layout.fillWidth: true
            title: "Virtualization Mode"
            
            ComboBox {
                id: virtModeCombo
                width: parent.width - 24
                model: virtModeManager
                textRole: "modeName"
                valueRole: "modeId"
                currentIndex: 1
                
                delegate: ItemDelegate {
                    width: virtModeCombo.width
                    height: 50
                    contentItem: ColumnLayout {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 16
                        spacing: 2
                        
                        Label {
                            text: model.modeName
                            font.pixelSize: 12
                            color: model.isAvailable ? 
                                      Material.color(Material.Blue, Material.Shade100) : 
                                      Material.color(Material.Grey, Material.Shade500)
                        }
                        
                        Label {
                            text: model.modeDescription
                            font.pixelSize: 9
                            color: Material.color(Material.Grey, Material.Shade400)
                            wrapMode: Text.WordWrap
                            maximumLineCount: 1
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                    
                    background: Rectangle {
                        color: index === virtModeCombo.currentIndex ? 
                                  Material.color(Material.Cyan, Material.Shade900) : 
                                  "transparent"
                    }
                }
                
                Label {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: -20
                    font.pixelSize: 10
                    color: Material.color(Material.Grey, Material.Shade500)
                    text: "Virtualization mode for this boot entry"
                }
            }
        }
        
        // Validation Errors
        Label {
            id: errorLabel
            Layout.fillWidth: true
            font.pixelSize: 11
            color: Material.color(Material.Red, Material.Shade300)
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }
        
        Item { Layout.fillHeight: true }
    }
    
    // File Dialogs
    FileDialog {
        id: kernelFileDialog
        title: "Select Kernel Image"
        folder: shortcuts.root
        nameFilters: ["Kernel files (*.bin *.img *.elf)", "All files (*)"]
        onAccepted: kernelField.text = selectedFile.toString()
    }
    
    FileDialog {
        id: initrdFileDialog
        title: "Select Initrd Image"
        folder: shortcuts.root
        nameFilters: ["Initrd files (*.img *.initrd *.initramfs)", "All files (*)"]
        onAccepted: initrdField.text = selectedFile.toString()
    }
    
    // Functions
    function validateInputs() {
        errorLabel.text = ""
        
        if (nameField.text.trim().length === 0) {
            errorLabel.text = "Error: Entry name is required"
            return false
        }
        
        if (kernelField.text.trim().length === 0) {
            errorLabel.text = "Error: Kernel path is required"
            return false
        }
        
        // Check if kernel file exists
        // Note: In real implementation, use Qt.fileExists or similar
        // For now, we'll skip this check
        
        return true
    }
    
    function appendCmdline(option) {
        if (cmdlineField.text.length > 0) {
            cmdlineField.text += " " + option
        } else {
            cmdlineField.text = option
        }
    }
    
    function findInitrd(kernelPath) {
        // Auto-detect initrd based on kernel path
        // This is a simplified implementation
        let baseName = kernelPath.replace(/.*\//, '').replace(/\..*$/, '')
        return "/boot/" + baseName + "-initrd.img"
    }
}

/*
 * Chip Component
 */
component Chip: Rectangle {
    property string text: ""
    
    implicitWidth: chipText.implicitWidth + 16
    implicitHeight: 24
    radius: 12
    color: hovered ? Material.color(Material.Blue, Material.Shade800) : 
                    Material.color(Material.Blue, Material.Shade900)
    border.color: Material.color(Material.Blue, Material.Shade500)
    border.width: 1
    
    property bool hovered: false
    
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: chip.hovered = true
        onExited: chip.hovered = false
        onClicked: chip.clicked()
    }
    
    signal clicked()
    
    Label {
        id: chipText
        anchors.centerIn: parent
        text: chip.text
        font.pixelSize: 10
        color: Material.color(Material.Blue, Material.Shade200)
    }
}
