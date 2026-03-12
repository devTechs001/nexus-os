import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material

ApplicationWindow {
    id: window
    width: 1024
    height: 768
    visible: true
    title: qsTr("NEXUS-OS Boot Manager")
    color: "#0d1117"
    
    Material.theme: Material.Dark
    Material.primary: "#58a6ff"
    Material.accent: "#e94560"
    
    // Header
    header: ToolBar {
        background: Rectangle { 
            color: "#161b22"
            border.color: "#30363d"
            border.width: 1
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 15
            
            Text {
                text: "🔷 NEXUS-OS"
                font.pixelSize: 24
                font.bold: true
                color: "#58a6ff"
            }
            
            Item { Layout.fillWidth: true }
            
            Text {
                text: "Boot Configuration Manager"
                color: "#8b949e"
                font.pixelSize: 14
            }
            
            Item { Layout.preferredWidth: 20 }
            
            Button {
                text: "💾 Save"
                
                background: Rectangle {
                    color: parent.pressed ? "#238636" : "#2ea043"
                    radius: 6
                }
                
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                }
                
                onClicked: {
                    bootConfig.applyConfig()
                    statusMessage.show("Configuration saved!", "success")
                }
            }
        }
    }
    
    // Footer with status
    footer: Pane {
        background: Rectangle { 
            color: "#161b22"
            border.color: "#30363d"
            border.width: 1
        }
        
        RowLayout {
            anchors.fill: parent
            
            Text {
                id: statusText
                text: "Ready"
                color: "#8b949e"
                font.pixelSize: 12
            }
            
            Item { Layout.fillWidth: true }
            
            Text {
                text: "Hardware: " + virtManager.hardwareVirtSupported ? "✓ Virtualization Supported" : "✗ No Virtualization Support"
                color: virtManager.hardwareVirtSupported ? "#3fb950" : "#f85149"
                font.pixelSize: 12
            }
        }
    }
    
    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        
        ColumnLayout {
            width: window.width - 40
            spacing: 20
            
            // Virtualization Mode Selection
            GroupBox {
                Layout.fillWidth: true
                title: "⚡ Virtualization Mode"
                
                background: Rectangle {
                    color: "#161b22"
                    radius: 10
                    border.color: "#30363d"
                    border.width: 1
                }
                
                label: Text {
                    text: parent.title
                    color: "#58a6ff"
                    font.pixelSize: 16
                    font.bold: true
                }
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10
                    
                    Repeater {
                        model: bootConfig.getVirtModes()
                        
                        delegate: Rectangle {
                            width: parent.width
                            height: 80
                            radius: 8
                            color: modelData.id === bootConfig.selectedVirtMode ? "#1f6feb33" : "#21262d"
                            border.color: modelData.id === bootConfig.selectedVirtMode ? "#58a6ff" : "#30363d"
                            border.width: 2
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 15
                                spacing: 15
                                
                                Text {
                                    text: modelData.icon
                                    font.pixelSize: 32
                                }
                                
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    
                                    Text {
                                        text: modelData.name
                                        font.pixelSize: 16
                                        font.bold: true
                                        color: "white"
                                    }
                                    
                                    Text {
                                        text: modelData.description
                                        font.pixelSize: 12
                                        color: "#8b949e"
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                    
                                    RowLayout {
                                        spacing: 15
                                        
                                        Text {
                                            text: "Overhead: " + modelData.overhead
                                            font.pixelSize: 11
                                            color: "#8b949e"
                                        }
                                        
                                        Text {
                                            text: "Security: " + modelData.security
                                            font.pixelSize: 11
                                            color: "#8b949e"
                                        }
                                        
                                        Rectangle {
                                            color: "#30363d"
                                            radius: 4
                                            height: 20
                                            width: recommendationLabel.implicitWidth + 10
                                            
                                            Text {
                                                id: recommendationLabel
                                                anchors.centerIn: parent
                                                text: modelData.recommended
                                                font.pixelSize: 10
                                                color: "#58a6ff"
                                            }
                                        }
                                    }
                                }
                                
                                RadioButton {
                                    checked: modelData.id === bootConfig.selectedVirtMode
                                    onClicked: bootConfig.selectedVirtMode = modelData.id
                                    
                                    indicator: Rectangle {
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        radius: 12
                                        color: "transparent"
                                        border.color: parent.checked ? "#58a6ff" : "#8b949e"
                                        border.width: 2
                                        
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 12
                                            height: 12
                                            radius: 6
                                            color: "#58a6ff"
                                            visible: parent.parent.checked
                                        }
                                    }
                                }
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: bootConfig.selectedVirtMode = modelData.id
                            }
                        }
                    }
                }
            }
            
            // Boot Options
            GroupBox {
                Layout.fillWidth: true
                title: "🔧 Boot Options"
                
                background: Rectangle {
                    color: "#161b22"
                    radius: 10
                    border.color: "#30363d"
                    border.width: 1
                }
                
                label: Text {
                    text: parent.title
                    color: "#58a6ff"
                    font.pixelSize: 16
                    font.bold: true
                }
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 15
                    
                    // Secure Boot
                    RowLayout {
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: "🔒 Secure Boot"
                                font.pixelSize: 14
                                font.bold: true
                                color: "white"
                            }
                            
                            Text {
                                text: "Verify boot chain integrity"
                                font.pixelSize: 11
                                color: "#8b949e"
                            }
                        }
                        
                        Switch {
                            checked: bootConfig.secureBoot
                            onCheckedChanged: bootConfig.secureBoot = checked
                            
                            indicator: Rectangle {
                                implicitWidth: 50
                                implicitHeight: 26
                                radius: 13
                                color: parent.checked ? "#2ea043" : "#30363d"
                                border.color: parent.checked ? "#2ea043" : "#8b949e"
                                border.width: 2
                                
                                Rectangle {
                                    x: parent.checked ? parent.width - width - 2 : 2
                                    y: 2
                                    width: 20
                                    height: 20
                                    radius: 10
                                    color: "white"
                                    Behavior on x {
                                        NumberAnimation { duration: 150 }
                                    }
                                }
                            }
                        }
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#30363d"
                    }
                    
                    // Fast Boot
                    RowLayout {
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: "⚡ Fast Boot"
                                font.pixelSize: 14
                                font.bold: true
                                color: "white"
                            }
                            
                            Text {
                                text: "Skip hardware checks for faster startup"
                                font.pixelSize: 11
                                color: "#8b949e"
                            }
                        }
                        
                        Switch {
                            checked: bootConfig.fastBoot
                            onCheckedChanged: bootConfig.fastBoot = checked
                            
                            indicator: Rectangle {
                                implicitWidth: 50
                                implicitHeight: 26
                                radius: 13
                                color: parent.checked ? "#2ea043" : "#30363d"
                                border.color: parent.checked ? "#2ea043" : "#8b949e"
                                border.width: 2
                                
                                Rectangle {
                                    x: parent.checked ? parent.width - width - 2 : 2
                                    y: 2
                                    width: 20
                                    height: 20
                                    radius: 10
                                    color: "white"
                                    Behavior on x {
                                        NumberAnimation { duration: 150 }
                                    }
                                }
                            }
                        }
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#30363d"
                    }
                    
                    // Verbose Boot
                    RowLayout {
                        Layout.fillWidth: true
                        
                        ColumnLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: "📝 Verbose Boot"
                                font.pixelSize: 14
                                font.bold: true
                                color: "white"
                            }
                            
                            Text {
                                text: "Show detailed boot messages"
                                font.pixelSize: 11
                                color: "#8b949e"
                            }
                        }
                        
                        Switch {
                            checked: bootConfig.verboseBoot
                            onCheckedChanged: bootConfig.verboseBoot = checked
                            
                            indicator: Rectangle {
                                implicitWidth: 50
                                implicitHeight: 26
                                radius: 13
                                color: parent.checked ? "#2ea043" : "#30363d"
                                border.color: parent.checked ? "#2ea043" : "#8b949e"
                                border.width: 2
                                
                                Rectangle {
                                    x: parent.checked ? parent.width - width - 2 : 2
                                    y: 2
                                    width: 20
                                    height: 20
                                    radius: 10
                                    color: "white"
                                    Behavior on x {
                                        NumberAnimation { duration: 150 }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Hardware Configuration
            GroupBox {
                Layout.fillWidth: true
                title: "🖥️ Hardware Configuration"
                
                background: Rectangle {
                    color: "#161b22"
                    radius: 10
                    border.color: "#30363d"
                    border.width: 1
                }
                
                label: Text {
                    text: parent.title
                    color: "#58a6ff"
                    font.pixelSize: 16
                    font.bold: true
                }
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 15
                    
                    // CPU Count
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        
                        RowLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: "CPU Cores: " + (bootConfig.cpuCount === 0 ? "All (" + Thread.idealThreadCount + ")" : bootConfig.cpuCount)
                                font.pixelSize: 14
                                color: "white"
                            }
                            
                            Item { Layout.fillWidth: true }
                            
                            Button {
                                text: "Reset"
                                implicitWidth: 80
                                
                                background: Rectangle {
                                    color: "#30363d"
                                    radius: 6
                                }
                                
                                contentItem: Text {
                                    text: parent.text
                                    color: "#8b949e"
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                
                                onClicked: bootConfig.cpuCount = 0
                            }
                        }
                        
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: Thread.idealThreadCount
                            stepSize: 1
                            value: bootConfig.cpuCount
                            onValueChanged: bootConfig.cpuCount = value
                            
                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 6
                                width: parent.availableWidth
                                radius: 3
                                color: "#30363d"
                                
                                Rectangle {
                                    width: parent.visualPosition * parent.width
                                    height: parent.height
                                    color: "#58a6ff"
                                    radius: 3
                                }
                            }
                            
                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                implicitWidth: 20
                                implicitHeight: 20
                                radius: 10
                                color: parent.pressed ? "#58a6ff" : "#1f6feb"
                            }
                        }
                    }
                    
                    // Memory Size
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        
                        RowLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: "Memory: " + (bootConfig.memorySize === 0 ? "All (" + Math.floor(QSysInfo.totalPhysicalMemory / 1024 / 1024) + " MB)" : bootConfig.memorySize + " MB")
                                font.pixelSize: 14
                                color: "white"
                            }
                            
                            Item { Layout.fillWidth: true }
                            
                            Button {
                                text: "Reset"
                                implicitWidth: 80
                                
                                background: Rectangle {
                                    color: "#30363d"
                                    radius: 6
                                }
                                
                                contentItem: Text {
                                    text: parent.text
                                    color: "#8b949e"
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                
                                onClicked: bootConfig.memorySize = 0
                            }
                        }
                        
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: Math.floor(QSysInfo.totalPhysicalMemory / 1024 / 1024)
                            stepSize: 512
                            value: bootConfig.memorySize
                            onValueChanged: bootConfig.memorySize = value
                            
                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 6
                                width: parent.availableWidth
                                radius: 3
                                color: "#30363d"
                                
                                Rectangle {
                                    width: parent.visualPosition * parent.width
                                    height: parent.height
                                    color: "#58a6ff"
                                    radius: 3
                                }
                            }
                            
                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                implicitWidth: 20
                                implicitHeight: 20
                                radius: 10
                                color: parent.pressed ? "#58a6ff" : "#1f6feb"
                            }
                        }
                    }
                }
            }
            
            // Security Profile
            GroupBox {
                Layout.fillWidth: true
                title: "🔐 Security Profile"
                
                background: Rectangle {
                    color: "#161b22"
                    radius: 10
                    border.color: "#30363d"
                    border.width: 1
                }
                
                label: Text {
                    text: parent.title
                    color: "#58a6ff"
                    font.pixelSize: 16
                    font.bold: true
                }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10
                    
                    Repeater {
                        model: ["minimal", "balanced", "maximum"]
                        
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            radius: 8
                            color: modelData === bootConfig.securityProfile ? "#1f6feb33" : "#21262d"
                            border.color: modelData === bootConfig.securityProfile ? "#58a6ff" : "#30363d"
                            border.width: 2
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 5
                                
                                Text {
                                    text: modelData === "minimal" ? "🔓" : modelData === "balanced" ? "🔒" : "🔐"
                                    font.pixelSize: 24
                                }
                                
                                Text {
                                    text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "white"
                                }
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: bootConfig.securityProfile = modelData
                            }
                        }
                    }
                }
            }
            
            // Action Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: 15
                
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    text: "💾 Save Configuration"
                    
                    background: Rectangle {
                        color: parent.pressed ? "#238636" : "#2ea043"
                        radius: 8
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    
                    onClicked: {
                        bootConfig.applyConfig()
                        statusMessage.show("Configuration saved successfully!", "success")
                    }
                }
                
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50
                    text: "🔄 Reset to Defaults"
                    
                    background: Rectangle {
                        color: parent.pressed ? "#da3633" : "#f85149"
                        radius: 8
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    
                    onClicked: {
                        bootConfig.resetToDefaults()
                        statusMessage.show("Reset to defaults", "info")
                    }
                }
            }
            
            // Recommendation
            GroupBox {
                Layout.fillWidth: true
                title: "💡 Recommendation"
                
                background: Rectangle {
                    color: "#161b22"
                    radius: 10
                    border.color: "#30363d"
                    border.width: 1
                }
                
                label: Text {
                    text: parent.title
                    color: "#58a6ff"
                    font.pixelSize: 16
                    font.bold: true
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    color: "#1f6feb22"
                    radius: 8
                    border.color: "#1f6feb"
                    border.width: 1
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 15
                        
                        Text {
                            text: "💡"
                            font.pixelSize: 32
                        }
                        
                        ColumnLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: "Based on your hardware (" + virtManager.getModeRecommendation().platform + "), we recommend:"
                                font.pixelSize: 12
                                color: "#8b949e"
                            }
                            
                            Text {
                                text: virtManager.getModeRecommendation().reason
                                font.pixelSize: 14
                                font.bold: true
                                color: "#58a6ff"
                            }
                        }
                    }
                }
            }
            
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 20
            }
        }
    }
    
    // Status message popup
    Popup {
        id: statusMessage
        property string messageType: "info"
        
        modal: true
        implicitWidth: 300
        implicitHeight: 80
        anchors.centerIn: parent
        background: Rectangle {
            color: statusMessage.messageType === "success" ? "#238636" : "#1f6feb"
            radius: 8
        }
        
        contentItem: Text {
            text: statusMessage.messageType === "success" ? "✓ " + statusMessage.title : statusMessage.title
            color: "white"
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        
        Timer {
            interval: 2000
            running: false
            onTriggered: statusMessage.close()
        }
        
        function show(text, type) {
            title = text
            messageType = type
            open()
            timer.start()
        }
    }
}
