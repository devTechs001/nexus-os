/*
 * NEXUS OS - Boot Manager GUI
 * Copyright (c) 2024 NEXUS Development Team
 *
 * Message.qml - Status Message Component
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: messageRoot
    width: 360
    height: 60
    
    property alias text: messageLabel.text
    property int duration: 3000
    property int messageType: Message.Info
    
    enum Type {
        Info,
        Success,
        Warning,
        Error
    }
    
    visible: false
    opacity: 0
    
    function show(msg, type) {
        text = msg
        messageType = type
        visible = true
        fadeAnim.start()
        hideTimer.start()
    }
    
    function hide() {
        fadeAnim.start()
    }
    
    Timer {
        id: hideTimer
        interval: duration
        onTriggered: {
            fadeAnim.start()
        }
    }
    
    SequentialAnimation {
        id: fadeAnim
        
        NumberAnimation {
            target: messageRoot
            property: "opacity"
            to: 1.0
            duration: 200
        }
        
        PauseAnimation {
            duration: duration - 400
        }
        
        NumberAnimation {
            target: messageRoot
            property: "opacity"
            to: 0.0
            duration: 200
            onFinished: messageRoot.visible = false
        }
    }
    
    Card {
        anchors.fill: parent
        Material.elevation: 4
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12
            
            // Icon
            Label {
                text: {
                    switch (messageType) {
                    case Message.Success: return "✓"
                    case Message.Warning: return "⚠"
                    case Message.Error: return "✗"
                    default: return "ℹ"
                    }
                }
                font.pixelSize: 20
            }
            
            // Message
            Label {
                id: messageLabel
                Layout.fillWidth: true
                font.pixelSize: 13
                color: {
                    switch (messageType) {
                    case Message.Success: return Material.color(Material.Green, Material.Shade100)
                    case Message.Warning: return Material.color(Material.Orange, Material.Shade100)
                    case Message.Error: return Material.color(Material.Red, Material.Shade100)
                    default: return Material.color(Material.Blue, Material.Shade100)
                    }
                }
                wrapMode: Text.WordWrap
            }
        }
        
        // Color indicator
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 3
            color: {
                switch (messageType) {
                case Message.Success: return Material.color(Material.Green)
                case Message.Warning: return Material.color(Material.Orange)
                case Message.Error: return Material.color(Material.Red)
                default: return Material.color(Material.Blue)
                }
            }
        }
    }
}
