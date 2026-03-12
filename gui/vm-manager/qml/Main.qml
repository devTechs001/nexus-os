import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtCharts

ApplicationWindow {
    id: window
    width: 1400
    height: 900
    visible: true
    title: qsTr("NEXUS VM Manager")
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
                text: "🖥️ NEXUS VM Manager"
                font.pixelSize: 24
                font.bold: true
                color: "#58a6ff"
            }
            
            Item { Layout.fillWidth: true }
            
            // Stats
            RowLayout {
                spacing: 20
                
                Rectangle {
                    width: 120
                    height: 40
                    color: "#23863633"
                    radius: 8
                    border.color: "#238636"
                    border.width: 1
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        
                        Text {
                            text: vmModel.runningCount + " Running"
                            color: "#3fb950"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        
                        Text {
                            text: vmModel.totalCount + " Total VMs"
                            color: "#8b949e"
                            font.pixelSize: 10
                        }
                    }
                }
                
                Rectangle {
                    width: 100
                    height: 40
                    color: "#1f6feb33"
                    radius: 8
                    border.color: "#1f6feb"
                    border.width: 1
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2
                        
                        Text {
                            text: "CPU: " + calculateTotalCPU() + "%"
                            color: "#58a6ff"
                            font.pixelSize: 12
                            font.bold: true
                        }
                        
                        Text {
                            text: "Memory: " + calculateTotalMemory() + " GB"
                            color: "#8b949e"
                            font.pixelSize: 10
                        }
                    }
                }
            }
            
            Item { Layout.preferredWidth: 20 }
            
            Button {
                text: "➕ Create VM"
                
                background: Rectangle {
                    color: parent.pressed ? "#238636" : "#2ea043"
                    radius: 6
                }
                
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }
                
                onClicked: createVMDialog.open()
            }
        }
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 15
        
        // Left Sidebar - VM List
        Rectangle {
            Layout.preferredWidth: 400
            Layout.fillHeight: true
            color: "#161b22"
            radius: 10
            border.color: "#30363d"
            border.width: 1
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 15
                
                // Search and Filter
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    
                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: "Search VMs..."
                        color: "white"
                        
                        background: Rectangle {
                            color: "#0d1117"
                            radius: 6
                            border.color: "#30363d"
                            border.width: 1
                        }
                    }
                    
                    ToolButton {
                        text: "🔍"
                        onClicked: vmModel.filterByStatus("")
                    }
                }
                
                // Filter Tabs
                RowLayout {
                    spacing: 5
                    
                    Repeater {
                        model: [
                            { name: "All", status: "all", color: "#58a6ff" },
                            { name: "Running", status: "running", color: "#3fb950" },
                            { name: "Stopped", status: "stopped", color: "#8b949e" },
                            { name: "Paused", status: "paused", color: "#d29922" }
                        ]
                        
                        delegate: Rectangle {
                            Layout.preferredHeight: 35
                            Layout.preferredWidth: filterText.implicitWidth + 20
                            radius: 6
                            color: model.status === currentFilter ? model.color + "33" : "transparent"
                            border.color: model.status === currentFilter ? model.color : "#30363d"
                            border.width: 1
                            
                            property string filterStatus: model.status
                            
                            Text {
                                id: filterText
                                anchors.centerIn: parent
                                text: model.name
                                color: model.status === currentFilter ? model.color : "#8b949e"
                                font.pixelSize: 12
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    currentFilter = model.status
                                    vmModel.filterByStatus(model.status)
                                }
                            }
                        }
                    }
                }
                
                // VM List
                ListView {
                    id: vmList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: vmModel
                    
                    delegate: Rectangle {
                        width: vmList.width - 10
                        height: 100
                        radius: 8
                        color: selectedVM === vmId ? "#1f6feb33" : "#21262d"
                        border.color: vmStatus === "running" ? "#3fb950" : 
                                      vmStatus === "paused" ? "#d29922" : "#30363d"
                        border.width: selectedVM === vmId ? 2 : 1
                        
                        property string vmId: model.vmId
                        property string vmName: model.vmName
                        property string vmStatus: model.vmStatus
                        property string vmType: model.vmType
                        property int vmCPU: model.vmCPU
                        property int vmMemory: model.vmMemory
                        property string vmUptime: model.vmUptime
                        property real vmCPUUsage: model.vmCPUUsage
                        property real vmMemoryUsage: model.vmMemoryUsage
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12
                            
                            // Status Indicator
                            Rectangle {
                                Layout.preferredSize: 40
                                radius: 8
                                color: vmStatus === "running" ? "#23863633" : 
                                       vmStatus === "paused" ? "#9e7a0333" : "#30363d"
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: vmStatus === "running" ? "▶" : 
                                          vmStatus === "paused" ? "⏸" : "⏹"
                                    font.pixelSize: 20
                                }
                            }
                            
                            // VM Info
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                
                                Text {
                                    text: vmName
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "white"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                
                                RowLayout {
                                    spacing: 10
                                    
                                    Rectangle {
                                        height: 18
                                        width: typeLabel.implicitWidth + 8
                                        radius: 4
                                        color: "#30363d"
                                        
                                        Text {
                                            id: typeLabel
                                            anchors.centerIn: parent
                                            text: vmType
                                            font.pixelSize: 10
                                            color: "#8b949e"
                                        }
                                    }
                                    
                                    Text {
                                        text: vmCPU + " vCPU"
                                        font.pixelSize: 11
                                        color: "#8b949e"
                                    }
                                    
                                    Text {
                                        text: (vmMemory / 1024).toFixed(1) + " GB"
                                        font.pixelSize: 11
                                        color: "#8b949e"
                                    }
                                }
                                
                                // Resource bars
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        
                                        RowLayout {
                                            Text {
                                                text: "CPU"
                                                font.pixelSize: 9
                                                color: "#8b949e"
                                            }
                                            Text {
                                                text: vmCPUUsage.toFixed(0) + "%"
                                                font.pixelSize: 9
                                                color: vmCPUUsage > 80 ? "#f85149" : "#8b949e"
                                            }
                                        }
                                        
                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 4
                                            radius: 2
                                            color: "#30363d"
                                            
                                            Rectangle {
                                                width: parent.width * (vmCPUUsage / 100)
                                                height: parent.height
                                                radius: 2
                                                color: vmCPUUsage > 80 ? "#f85149" : "#58a6ff"
                                            }
                                        }
                                    }
                                    
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        
                                        RowLayout {
                                            Text {
                                                text: "MEM"
                                                font.pixelSize: 9
                                                color: "#8b949e"
                                            }
                                            Text {
                                                text: vmMemoryUsage.toFixed(0) + "%"
                                                font.pixelSize: 9
                                                color: vmMemoryUsage > 80 ? "#f85149" : "#8b949e"
                                            }
                                        }
                                        
                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 4
                                            radius: 2
                                            color: "#30363d"
                                            
                                            Rectangle {
                                                width: parent.width * (vmMemoryUsage / 100)
                                                height: parent.height
                                                radius: 2
                                                color: vmMemoryUsage > 80 ? "#f85149" : "#a371f7"
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // Quick Actions
                            ColumnLayout {
                                spacing: 5
                                
                                ToolButton {
                                    visible: vmStatus !== "running"
                                    text: "▶"
                                    font.pixelSize: 16
                                    onClicked: {
                                        selectedVM = vmId
                                        vmController.startVM(vmId)
                                    }
                                }
                                
                                ToolButton {
                                    visible: vmStatus === "running"
                                    text: "⏸"
                                    font.pixelSize: 16
                                    onClicked: {
                                        selectedVM = vmId
                                        vmController.pauseVM(vmId)
                                    }
                                }
                                
                                ToolButton {
                                    visible: vmStatus !== "stopped"
                                    text: "⏹"
                                    font.pixelSize: 16
                                    onClicked: {
                                        selectedVM = vmId
                                        vmController.stopVM(vmId)
                                    }
                                }
                                
                                ToolButton {
                                    text: "⋮"
                                    font.pixelSize: 16
                                    onClicked: {
                                        selectedVM = vmId
                                        contextMenu.popup()
                                    }
                                }
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: selectedVM = vmId
                            onDoubleClicked: vmController.openConsole(vmId)
                        }
                        
                        // Context Menu
                        Menu {
                            id: contextMenu
                            
                            MenuItem {
                                text: "🖥️ Open Console"
                                onTriggered: vmController.openConsole(vmId)
                            }
                            
                            MenuItem {
                                text: "⚙️ Settings"
                                onTriggered: vmController.openSettings(vmId)
                            }
                            
                            MenuSeparator {}
                            
                            MenuItem {
                                text: "📸 Create Snapshot"
                                onTriggered: snapshotDialog.open()
                            }
                            
                            MenuItem {
                                text: "🔄 Restart"
                                onTriggered: vmController.restartVM(vmId)
                            }
                            
                            MenuSeparator {}
                            
                            MenuItem {
                                text: "📤 Export"
                                onTriggered: exportDialog.open()
                            }
                            
                            MenuItem {
                                text: "🗑️ Delete"
                                onTriggered: deleteDialog.open()
                            }
                        }
                    }
                    
                    ScrollBar.vertical: ScrollBar { active: true }
                }
            }
        }
        
        // Right Panel - Details and Charts
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#161b22"
            radius: 10
            border.color: "#30363d"
            border.width: 1
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 15
                
                // Selected VM Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    color: "#21262d"
                    radius: 8
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 15
                        
                        Rectangle {
                            Layout.preferredSize: 50
                            radius: 10
                            color: "#1f6feb"
                            
                            Text {
                                anchors.centerIn: parent
                                text: selectedVMName ? selectedVMName.charAt(0).toUpperCase() : "?"
                                font.pixelSize: 24
                                font.bold: true
                                color: "white"
                            }
                        }
                        
                        ColumnLayout {
                            Layout.fillWidth: true
                            
                            Text {
                                text: selectedVMName || "Select a VM"
                                font.pixelSize: 18
                                font.bold: true
                                color: "white"
                            }
                            
                            Text {
                                text: selectedVMStatus ? selectedVMStatus.toUpperCase() : ""
                                font.pixelSize: 12
                                color: selectedVMStatus === "running" ? "#3fb950" : 
                                       selectedVMStatus === "paused" ? "#d29922" : "#8b949e"
                            }
                            
                            Text {
                                text: selectedVMUptime ? "Uptime: " + selectedVMUptime : ""
                                font.pixelSize: 11
                                color: "#8b949e"
                            }
                        }
                        
                        // Action Buttons
                        RowLayout {
                            spacing: 10
                            
                            Button {
                                visible: selectedVMStatus !== "running"
                                text: "Start"
                                
                                background: Rectangle {
                                    color: parent.pressed ? "#238636" : "#2ea043"
                                    radius: 6
                                }
                                
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                
                                onClicked: vmController.startVM(selectedVM)
                            }
                            
                            Button {
                                visible: selectedVMStatus === "running"
                                text: "Stop"
                                
                                background: Rectangle {
                                    color: parent.pressed ? "#da3633" : "#f85149"
                                    radius: 6
                                }
                                
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                
                                onClicked: vmController.stopVM(selectedVM)
                            }
                            
                            Button {
                                text: "Console"
                                
                                background: Rectangle {
                                    color: "#30363d"
                                    radius: 6
                                }
                                
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                
                                onClicked: vmController.openConsole(selectedVM)
                            }
                        }
                    }
                }
                
                // Charts Row
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 15
                    
                    // CPU Chart
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#21262d"
                        radius: 8
                        
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8
                            
                            Text {
                                text: "📊 CPU Usage"
                                font.pixelSize: 14
                                font.bold: true
                                color: "#58a6ff"
                            }
                            
                            ChartView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                backgroundColor: "#0d1117"
                                legendVisible: false
                                
                                ValueAxis {
                                    id: cpuXAxis
                                    min: 0
                                    max: 10
                                }
                                
                                ValueAxis {
                                    id: cpuYAxis
                                    min: 0
                                    max: 100
                                }
                                
                                LineSeries {
                                    id: cpuSeries
                                    axisX: cpuXAxis
                                    axisY: cpuYAxis
                                    
                                    ColorGradient {
                                        stops: [
                                            GradientStop { position: 0.0; color: "#58a6ff" },
                                            GradientStop { position: 1.0; color: "#1f6feb" }
                                        ]
                                    }
                                }
                            }
                        }
                    }
                    
                    // Memory Chart
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#21262d"
                        radius: 8
                        
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8
                            
                            Text {
                                text: "📈 Memory Usage"
                                font.pixelSize: 14
                                font.bold: true
                                color: "#a371f7"
                            }
                            
                            ChartView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                backgroundColor: "#0d1117"
                                legendVisible: false
                                
                                ValueAxis {
                                    id: memXAxis
                                    min: 0
                                    max: 10
                                }
                                
                                ValueAxis {
                                    id: memYAxis
                                    min: 0
                                    max: 100
                                }
                                
                                LineSeries {
                                    id: memSeries
                                    axisX: memXAxis
                                    axisY: memYAxis
                                    
                                    ColorGradient {
                                        stops: [
                                            GradientStop { position: 0.0; color: "#a371f7" },
                                            GradientStop { position: 1.0; color: "#58a6ff" }
                                        ]
                                    }
                                }
                            }
                        }
                    }
                }
                
                // VM Details
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    color: "#21262d"
                    radius: 8
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10
                        
                        Text {
                            text: "⚙️ VM Configuration"
                            font.pixelSize: 14
                            font.bold: true
                            color: "#58a6ff"
                        }
                        
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 4
                            rowSpacing: 10
                            columnSpacing: 10
                            
                            Repeater {
                                model: [
                                    { label: "vCPUs", value: selectedVMCPU || 0, icon: "💻" },
                                    { label: "Memory", value: (selectedVMMemory / 1024).toFixed(2) + " GB", icon: "📦" },
                                    { label: "Disk", value: selectedVMDisk + " GB", icon: "💾" },
                                    { label: "Network", value: selectedVMNetwork || "N/A", icon: "🌐" },
                                    { label: "Type", value: selectedVMType || "-", icon: "🔷" },
                                    { label: "OS", value: selectedVMOS || "-", icon: "🖥️" },
                                    { label: "Created", value: selectedVMCreated || "-", icon: "📅" },
                                    { label: "ID", value: selectedVM || "-", icon: "🏷️" }
                                ]
                                
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 50
                                    color: "#0d1117"
                                    radius: 6
                                    
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        
                                        Text {
                                            text: modelData.icon
                                            font.pixelSize: 20
                                        }
                                        
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            
                                            Text {
                                                text: modelData.label
                                                font.pixelSize: 10
                                                color: "#8b949e"
                                            }
                                            
                                            Text {
                                                text: modelData.value
                                                font.pixelSize: 12
                                                font.bold: true
                                                color: "#e6edf3"
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Create VM Dialog
    Dialog {
        id: createVMDialog
        title: "Create New Virtual Machine"
        modal: true
        parent: Overlay.overlay
        
        background: Rectangle {
            color: "#161b22"
            radius: 12
            border.color: "#30363d"
            border.width: 1
        }
        
        ColumnLayout {
            Layout.preferredWidth: 500
            spacing: 15
            
            // VM Name
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                
                Text {
                    text: "VM Name"
                    color: "#8b949e"
                    font.pixelSize: 12
                }
                
                TextField {
                    id: newVMName
                    Layout.fillWidth: true
                    placeholderText: "Enter VM name"
                    color: "white"
                    
                    background: Rectangle {
                        color: "#0d1117"
                        radius: 6
                        border.color: "#30363d"
                        border.width: 1
                    }
                }
            }
            
            // VM Type
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                
                Text {
                    text: "Virtualization Type"
                    color: "#8b949e"
                    font.pixelSize: 12
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    
                    Repeater {
                        model: [
                            { type: "process", name: "Process", icon: "🔷" },
                            { type: "hardware", name: "Hardware", icon: "🖥️" },
                            { type: "container", name: "Container", icon: "📦" },
                            { type: "enclave", name: "Enclave", icon: "🔒" }
                        ]
                        
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70
                            radius: 8
                            color: newVMType === modelData.type ? "#1f6feb33" : "#21262d"
                            border.color: newVMType === modelData.type ? "#58a6ff" : "#30363d"
                            border.width: 2
                            
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 5
                                
                                Text {
                                    text: modelData.icon
                                    font.pixelSize: 24
                                }
                                
                                Text {
                                    text: modelData.name
                                    color: newVMType === modelData.type ? "white" : "#8b949e"
                                    font.pixelSize: 12
                                }
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: newVMType = modelData.type
                            }
                        }
                    }
                }
            }
            
            // Resources
            RowLayout {
                Layout.fillWidth: true
                spacing: 15
                
                // CPU
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    
                    Text {
                        text: "vCPUs: " + newVMCPU
                        color: "#8b949e"
                        font.pixelSize: 12
                    }
                    
                    Slider {
                        id: cpuSlider
                        Layout.fillWidth: true
                        from: 1
                        to: 16
                        stepSize: 1
                        value: 2
                        
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
                            color: "#1f6feb"
                        }
                        
                        onValueChanged: newVMCPU = value
                    }
                }
                
                // Memory
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    
                    Text {
                        text: "Memory: " + newVMMemory + " MB"
                        color: "#8b949e"
                        font.pixelSize: 12
                    }
                    
                    Slider {
                        id: memorySlider
                        Layout.fillWidth: true
                        from: 512
                        to: 32768
                        stepSize: 512
                        value: 4096
                        
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
                                color: "#a371f7"
                                radius: 3
                            }
                        }
                        
                        handle: Rectangle {
                            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                            implicitWidth: 20
                            implicitHeight: 20
                            radius: 10
                            color: "#a371f7"
                        }
                        
                        onValueChanged: newVMMemory = value
                    }
                }
            }
            
            // Disk Size
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                
                Text {
                    text: "Disk Size: " + newVMDisk + " GB"
                    color: "#8b949e"
                    font.pixelSize: 12
                }
                
                Slider {
                    id: diskSlider
                    Layout.fillWidth: true
                    from: 16
                    to: 1024
                    stepSize: 16
                    value: 64
                    
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
                            color: "#238636"
                            radius: 3
                        }
                    }
                    
                    handle: Rectangle {
                        x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 20
                        implicitHeight: 20
                        radius: 10
                        color: "#238636"
                    }
                    
                    onValueChanged: newVMDisk = value
                }
            }
            
            // Buttons
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10
                
                Button {
                    text: "Cancel"
                    
                    background: Rectangle {
                        color: "#30363d"
                        radius: 6
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "#8b949e"
                        horizontalAlignment: Text.AlignHCenter
                    }
                    
                    onClicked: createVMDialog.close()
                }
                
                Button {
                    text: "Create VM"
                    
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
                        var config = {
                            "id": "vm-" + Date.now(),
                            "name": newVMName.text,
                            "type": newVMType,
                            "cpu": newVMCPU,
                            "memory": newVMMemory,
                            "disk": newVMDisk,
                            "status": "stopped"
                        }
                        vmModel.addVM(config)
                        vmController.createVM(config)
                        createVMDialog.close()
                        resetCreateForm()
                    }
                }
            }
        }
    }
    
    // Delete Confirmation Dialog
    Dialog {
        id: deleteDialog
        title: "Delete VM"
        modal: true
        parent: Overlay.overlay
        
        background: Rectangle {
            color: "#161b22"
            radius: 12
            border.color: "#30363d"
            border.width: 1
        }
        
        ColumnLayout {
            spacing: 15
            
            Text {
                text: "Are you sure you want to delete this VM? This action cannot be undone."
                color: "#8b949e"
                wrapMode: Text.WordWrap
                Layout.preferredWidth: 400
            }
            
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10
                
                Button {
                    text: "Cancel"
                    onClicked: deleteDialog.close()
                    
                    background: Rectangle {
                        color: "#30363d"
                        radius: 6
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "#8b949e"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
                
                Button {
                    text: "Delete"
                    
                    background: Rectangle {
                        color: parent.pressed ? "#da3633" : "#f85149"
                        radius: 6
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                    }
                    
                    onClicked: {
                        vmModel.removeVM(selectedVM)
                        vmController.deleteVM(selectedVM)
                        deleteDialog.close()
                        selectedVM = ""
                    }
                }
            }
        }
    }
    
    // Snapshot Dialog
    Dialog {
        id: snapshotDialog
        title: "Create Snapshot"
        modal: true
        parent: Overlay.overlay
        
        background: Rectangle {
            color: "#161b22"
            radius: 12
            border.color: "#30363d"
            border.width: 1
        }
        
        ColumnLayout {
            spacing: 15
            
            TextField {
                id: snapshotName
                Layout.preferredWidth: 300
                placeholderText: "Snapshot name"
                color: "white"
                
                background: Rectangle {
                    color: "#0d1117"
                    radius: 6
                    border.color: "#30363d"
                    border.width: 1
                }
            }
            
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10
                
                Button {
                    text: "Cancel"
                    onClicked: snapshotDialog.close()
                    
                    background: Rectangle {
                        color: "#30363d"
                        radius: 6
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "#8b949e"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
                
                Button {
                    text: "Create"
                    
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
                        vmController.createSnapshot(selectedVM, snapshotName.text)
                        snapshotDialog.close()
                    }
                }
            }
        }
    }
    
    // Export Dialog
    Dialog {
        id: exportDialog
        title: "Export VM"
        modal: true
        parent: Overlay.overlay
        
        background: Rectangle {
            color: "#161b22"
            radius: 12
            border.color: "#30363d"
            border.width: 1
        }
        
        ColumnLayout {
            spacing: 15
            
            TextField {
                id: exportPath
                Layout.preferredWidth: 400
                placeholderText: "Export path"
                text: "/exports/" + selectedVMName + ".vm"
                color: "white"
                
                background: Rectangle {
                    color: "#0d1117"
                    radius: 6
                    border.color: "#30363d"
                    border.width: 1
                }
            }
            
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10
                
                Button {
                    text: "Cancel"
                    onClicked: exportDialog.close()
                    
                    background: Rectangle {
                        color: "#30363d"
                        radius: 6
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "#8b949e"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
                
                Button {
                    text: "Export"
                    
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
                        vmController.exportVM(selectedVM, exportPath.text)
                        exportDialog.close()
                    }
                }
            }
        }
    }
    
    property string selectedVM: ""
    property string currentFilter: "all"
    property string newVMType: "process"
    property int newVMCPU: 2
    property int newVMMemory: 4096
    property int newVMDisk: 64
    
    // Selected VM properties (computed from model)
    property string selectedVMName: ""
    property string selectedVMStatus: ""
    property string selectedVMType: ""
    property int selectedVMCPU: 0
    property int selectedVMMemory: 0
    property int selectedVMDisk: 0
    property string selectedVMUptime: ""
    property string selectedVMNetwork: ""
    property string selectedVMOS: ""
    property string selectedVMCreated: ""
    
    onSelectedVMChanged: {
        if (selectedVM) {
            var vm = vmModel.getVM(selectedVM)
            selectedVMName = vm.name
            selectedVMStatus = vm.status
            selectedVMType = vm.type
            selectedVMCPU = vm.cpu
            selectedVMMemory = vm.memory
            selectedVMDisk = vm.disk
        }
    }
    
    function calculateTotalCPU() {
        var total = 0
        for (var i = 0; i < vmModel.rowCount(); i++) {
            total += vmModel.data(vmModel.index(i, 0), vmModel.CPUUsageRole)
        }
        return total.toFixed(1)
    }
    
    function calculateTotalMemory() {
        var total = 0
        for (var i = 0; i < vmModel.rowCount(); i++) {
            total += vmModel.data(vmModel.index(i, 0), vmModel.MemorySizeRole) / 1024
        }
        return total.toFixed(1)
    }
    
    function resetCreateForm() {
        newVMName.text = ""
        newVMType = "process"
        newVMCPU = 2
        newVMMemory = 4096
        newVMDisk = 64
    }
    
    // Update charts periodically
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: updateCharts()
    }
    
    function updateCharts() {
        // Update CPU chart with random data for demo
        cpuSeries.clear()
        memSeries.clear()
        
        var now = Date.now()
        for (var i = 0; i < 10; i++) {
            var cpuVal = 20 + Math.random() * 60
            var memVal = 40 + Math.random() * 40
            cpuSeries.append(i, cpuVal)
            memSeries.append(i, memVal)
        }
    }
    
    Component.onCompleted: {
        updateCharts()
    }
}
