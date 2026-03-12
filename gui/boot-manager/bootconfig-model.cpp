/*
 * NEXUS OS - Boot Manager GUI
 * Copyright (c) 2024 NEXUS Development Team
 *
 * bootconfig-model.cpp - Boot Configuration Data Model Implementation
 */

#include "bootconfig-model.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDirIterator>
#include <QMessageBox>
#include <QDesktopServices>

/*===========================================================================*/
/*                         CONSTRUCTOR/DESTRUCTOR                            */
/*===========================================================================*/

BootConfigModel::BootConfigModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_globalTimeout(10)
    , m_modified(false)
{
    // Determine config path
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    m_configPath = configDir + "/boot-config.json";
    
    // Register metatype
    qRegisterMetaType<BootEntry>("BootEntry");
}

BootConfigModel::~BootConfigModel()
{
    if (m_modified) {
        saveConfiguration();
    }
}

/*===========================================================================*/
/*                         QABSTRACTLISTMODEL INTERFACE                      */
/*===========================================================================*/

int BootConfigModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_entries.size();
}

QVariant BootConfigModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }
    
    const BootEntry &entry = m_entries.at(index.row());
    
    switch (role) {
    case IdRole:
        return entry.id;
    case NameRole:
        return entry.name;
    case KernelPathRole:
        return entry.kernelPath;
    case InitrdPathRole:
        return entry.initrdPath;
    case CmdlineRole:
        return entry.cmdline;
    case UuidRole:
        return entry.uuid;
    case IsDefaultRole:
        return entry.isDefault;
    case IsEnabledRole:
        return entry.isEnabled;
    case TimeoutRole:
        return entry.timeout;
    case VirtModeRole:
        return entry.virtMode;
    case LastBootedRole:
        return entry.lastBooted;
    case BootCountRole:
        return entry.bootCount;
    case Qt::DisplayRole:
        return entry.name;
    default:
        return QVariant();
    }
}

bool BootConfigModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return false;
    }
    
    BootEntry &entry = m_entries[index.row()];
    bool changed = false;
    
    switch (role) {
    case NameRole:
        changed = (entry.name != value.toString());
        entry.name = value.toString();
        break;
    case KernelPathRole:
        changed = (entry.kernelPath != value.toString());
        entry.kernelPath = value.toString();
        break;
    case InitrdPathRole:
        changed = (entry.initrdPath != value.toString());
        entry.initrdPath = value.toString();
        break;
    case CmdlineRole:
        changed = (entry.cmdline != value.toString());
        entry.cmdline = value.toString();
        break;
    case IsEnabledRole:
        changed = (entry.isEnabled != value.toBool());
        entry.isEnabled = value.toBool();
        break;
    case TimeoutRole:
        changed = (entry.timeout != value.toInt());
        entry.timeout = value.toInt();
        break;
    case VirtModeRole:
        changed = (entry.virtMode != value.toString());
        entry.virtMode = value.toString();
        break;
    }
    
    if (changed) {
        m_modified = true;
        emit dataChanged(index, index, {role});
        emit entryModified(index.row());
        return true;
    }
    
    return false;
}

Qt::ItemFlags BootConfigModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> BootConfigModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "entryId";
    roles[NameRole] = "entryName";
    roles[KernelPathRole] = "kernelPath";
    roles[InitrdPathRole] = "initrdPath";
    roles[CmdlineRole] = "cmdline";
    roles[UuidRole] = "uuid";
    roles[IsDefaultRole] = "isDefault";
    roles[IsEnabledRole] = "isEnabled";
    roles[TimeoutRole] = "timeout";
    roles[VirtModeRole] = "virtMode";
    roles[LastBootedRole] = "lastBooted";
    roles[BootCountRole] = "bootCount";
    return roles;
}

/*===========================================================================*/
/*                         MODEL MANAGEMENT                                  */
/*===========================================================================*/

int BootConfigModel::defaultIndex() const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).isDefault) {
            return i;
        }
    }
    return m_entries.isEmpty() ? -1 : 0;
}

void BootConfigModel::setGlobalTimeout(int timeout)
{
    if (m_globalTimeout != timeout) {
        m_globalTimeout = qMax(0, timeout);
        m_modified = true;
        emit timeoutChanged();
    }
}

/*===========================================================================*/
/*                         CONFIGURATION LOADING/SAVING                      */
/*===========================================================================*/

bool BootConfigModel::loadConfiguration()
{
    QFile file(m_configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        // Create default configuration
        createDefaultEntries();
        return true;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError) {
        qWarning("Failed to parse boot config: %s", error.errorString().toUtf8().constData());
        createDefaultEntries();
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Load global settings
    m_globalTimeout = root["globalTimeout"].toInt(10);
    
    // Load entries
    beginResetModel();
    m_entries.clear();
    
    QJsonArray entriesArray = root["entries"].toArray();
    for (const QJsonValue &value : entriesArray) {
        BootEntry entry = BootEntry::fromJson(value.toObject());
        m_entries.append(entry);
    }
    
    // Ensure at least one default entry
    if (m_entries.isEmpty()) {
        createDefaultEntries();
    } else {
        bool hasDefault = false;
        for (const BootEntry &entry : m_entries) {
            if (entry.isDefault) {
                hasDefault = true;
                break;
            }
        }
        if (!hasDefault && !m_entries.isEmpty()) {
            m_entries[0].isDefault = true;
        }
    }
    
    endResetModel();
    m_modified = false;
    
    emit countChanged();
    emit defaultChanged();
    
    return true;
}

bool BootConfigModel::saveConfiguration()
{
    QJsonObject root;
    root["globalTimeout"] = m_globalTimeout;
    
    QJsonArray entriesArray;
    for (const BootEntry &entry : m_entries) {
        entriesArray.append(entry.toJson());
    }
    root["entries"] = entriesArray;
    
    QJsonDocument doc(root);
    
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Failed to save boot config: %s", file.errorString().toUtf8().constData());
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    m_modified = false;
    return true;
}

void BootConfigModel::reloadConfiguration()
{
    if (m_modified) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr, "Unsaved Changes",
            "You have unsaved changes. Discard them and reload?",
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    loadConfiguration();
}

/*===========================================================================*/
/*                         ENTRY MANAGEMENT                                  */
/*===========================================================================*/

int BootConfigModel::addEntry(const QString &name, const QString &kernelPath,
                              const QString &initrdPath, const QString &cmdline)
{
    BootEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.name = name;
    entry.kernelPath = kernelPath;
    entry.initrdPath = initrdPath.isEmpty() ? findInitrdForKernel(kernelPath) : initrdPath;
    entry.cmdline = cmdline;
    entry.uuid = generateUuid();
    entry.isEnabled = true;
    entry.isDefault = m_entries.isEmpty();
    entry.virtMode = "light";
    
    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append(entry);
    endInsertRows();
    
    m_modified = true;
    emit countChanged();
    emit entryAdded(m_entries.size() - 1);
    
    return m_entries.size() - 1;
}

bool BootConfigModel::removeEntry(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }
    
    // Don't remove last entry
    if (m_entries.size() == 1) {
        return false;
    }
    
    BootEntry entry = m_entries.at(index);
    
    beginRemoveRows(QModelIndex(), index, index);
    m_entries.removeAt(index);
    endRemoveRows();
    
    // If we removed the default entry, set a new default
    if (entry.isDefault && !m_entries.isEmpty()) {
        m_entries[0].isDefault = true;
        emit defaultChanged();
    }
    
    m_modified = true;
    emit countChanged();
    emit entryRemoved(index);
    
    return true;
}

bool BootConfigModel::removeEntryById(const QString &id)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            return removeEntry(i);
        }
    }
    return false;
}

BootEntry BootConfigModel::getEntry(int index) const
{
    if (index < 0 || index >= m_entries.size()) {
        return BootEntry();
    }
    return m_entries.at(index);
}

BootEntry BootConfigModel::getEntryById(const QString &id) const
{
    for (const BootEntry &entry : m_entries) {
        if (entry.id == id) {
            return entry;
        }
    }
    return BootEntry();
}

bool BootConfigModel::updateEntry(int index, const BootEntry &entry)
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }
    
    m_entries[index] = entry;
    m_modified = true;
    
    QModelIndex idx = createIndex(index, 0);
    emit dataChanged(idx, idx);
    emit entryModified(index);
    
    return true;
}

void BootConfigModel::moveEntry(int from, int to)
{
    if (from < 0 || from >= m_entries.size() ||
        to < 0 || to >= m_entries.size() || from == to) {
        return;
    }
    
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
    BootEntry entry = m_entries.takeAt(from);
    m_entries.insert(to, entry);
    endMoveRows();
    
    m_modified = true;
}

/*===========================================================================*/
/*                         DEFAULT ENTRY                                     */
/*===========================================================================*/

void BootConfigModel::setDefaultEntry(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }
    
    for (int i = 0; i < m_entries.size(); ++i) {
        m_entries[i].isDefault = (i == index);
    }
    
    m_modified = true;
    emit defaultChanged();
}

void BootConfigModel::setDefaultEntryById(const QString &id)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id) {
            setDefaultEntry(i);
            return;
        }
    }
}

/*===========================================================================*/
/*                         ENTRY STATE                                       */
/*===========================================================================*/

void BootConfigModel::setEntryEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }
    
    setData(createIndex(index, 0), enabled, IsEnabledRole);
}

bool BootConfigModel::isEntryEnabled(int index) const
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }
    return m_entries.at(index).isEnabled;
}

/*===========================================================================*/
/*                         BOOT TRACKING                                     */
/*===========================================================================*/

void BootConfigModel::recordBoot(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }
    
    BootEntry &entry = m_entries[index];
    entry.lastBooted = QDateTime::currentDateTime().toString(Qt::ISODate);
    entry.bootCount++;
    
    m_modified = true;
    
    QModelIndex idx = createIndex(index, 0);
    emit dataChanged(idx, idx, {LastBootedRole, BootCountRole});
    emit entryModified(index);
}

void BootConfigModel::clearBootHistory()
{
    for (BootEntry &entry : m_entries) {
        entry.lastBooted.clear();
        entry.bootCount = 0;
    }
    
    m_modified = true;
    emit dataChanged(createIndex(0, 0), createIndex(m_entries.size() - 1, 0));
}

/*===========================================================================*/
/*                         VALIDATION                                        */
/*===========================================================================*/

bool BootConfigModel::validateEntry(int index) const
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }
    
    const BootEntry &entry = m_entries.at(index);
    
    // Check kernel path exists
    if (!validatePath(entry.kernelPath)) {
        return false;
    }
    
    // Check initrd path if specified
    if (!entry.initrdPath.isEmpty() && !validatePath(entry.initrdPath)) {
        return false;
    }
    
    // Check name is not empty
    if (entry.name.trimmed().isEmpty()) {
        return false;
    }
    
    return true;
}

bool BootConfigModel::validatePath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }
    
    QFileInfo fi(path);
    return fi.exists() && fi.isFile();
}

QStringList BootConfigModel::scanForKernels(const QString &directory) const
{
    QStringList kernels;
    QDir dir(directory);
    
    if (!dir.exists()) {
        return kernels;
    }
    
    // Look for kernel files
    QStringList filters;
    filters << "vmlinuz*" << "bzImage*" << "kernel*" << "NEXUS-KERNEL*";
    
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::Readable);
    
    QFileInfoList files = dir.entryInfoList();
    for (const QFileInfo &fi : files) {
        kernels.append(fi.absoluteFilePath());
    }
    
    return kernels;
}

/*===========================================================================*/
/*                         CONFIGURATION EDITOR                              */
============================================================================*/

void BootConfigModel::showConfigurationEditor()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString configFile = configDir + "/boot-config.json";
    
    QDesktopServices::openUrl(QUrl::fromLocalFile(configFile));
}

/*===========================================================================*/
/*                         IMPORT/EXPORT                                     */
/*===========================================================================*/

bool BootConfigModel::importConfiguration(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError) {
        return false;
    }
    
    QJsonObject root = doc.object();
    
    beginResetModel();
    m_entries.clear();
    
    m_globalTimeout = root["globalTimeout"].toInt(10);
    
    QJsonArray entriesArray = root["entries"].toArray();
    for (const QJsonValue &value : entriesArray) {
        BootEntry entry = BootEntry::fromJson(value.toObject());
        m_entries.append(entry);
    }
    
    endResetModel();
    
    m_modified = true;
    emit countChanged();
    emit defaultChanged();
    
    return true;
}

bool BootConfigModel::exportConfiguration(const QString &filePath)
{
    QJsonObject root;
    root["globalTimeout"] = m_globalTimeout;
    
    QJsonArray entriesArray;
    for (const BootEntry &entry : m_entries) {
        entriesArray.append(entry.toJson());
    }
    root["entries"] = entriesArray;
    
    QJsonDocument doc(root);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

/*===========================================================================*/
/*                         RESET/CLEAR                                       */
/*===========================================================================*/

void BootConfigModel::resetToDefaults()
{
    beginResetModel();
    m_entries.clear();
    m_globalTimeout = 10;
    createDefaultEntries();
    endResetModel();
    
    m_modified = true;
    emit countChanged();
    emit defaultChanged();
}

void BootConfigModel::clearAll()
{
    if (m_entries.isEmpty()) {
        return;
    }
    
    beginResetModel();
    m_entries.clear();
    createDefaultEntries();
    endResetModel();
    
    m_modified = true;
    emit countChanged();
}

/*===========================================================================*/
/*                         PRIVATE HELPERS                                   */
/*===========================================================================*/

void BootConfigModel::createDefaultEntries()
{
    // NEXUS OS Default
    BootEntry nexusEntry;
    nexusEntry.id = "nexus-default";
    nexusEntry.name = "NEXUS OS";
    nexusEntry.kernelPath = "/boot/nexus-kernel";
    nexusEntry.initrdPath = "/boot/nexus-initrd.img";
    nexusEntry.cmdline = "quiet splash loglevel=3";
    nexusEntry.uuid = generateUuid();
    nexusEntry.isDefault = true;
    nexusEntry.isEnabled = true;
    nexusEntry.timeout = 0;
    nexusEntry.virtMode = "light";
    nexusEntry.bootCount = 0;
    m_entries.append(nexusEntry);
    
    // Safe Mode
    BootEntry safeEntry;
    safeEntry.id = "nexus-safe";
    safeEntry.name = "NEXUS OS (Safe Mode)";
    safeEntry.kernelPath = "/boot/nexus-kernel";
    safeEntry.initrdPath = "/boot/nexus-initrd.img";
    safeEntry.cmdline = "nomodeset nosmp noapic nolapic";
    safeEntry.uuid = generateUuid();
    safeEntry.isDefault = false;
    safeEntry.isEnabled = true;
    safeEntry.timeout = 0;
    safeEntry.virtMode = "native";
    safeEntry.bootCount = 0;
    m_entries.append(safeEntry);
    
    // Debug Mode
    BootEntry debugEntry;
    debugEntry.id = "nexus-debug";
    debugEntry.name = "NEXUS OS (Debug Mode)";
    debugEntry.kernelPath = "/boot/nexus-kernel-debug";
    debugEntry.initrdPath = "/boot/nexus-initrd-debug.img";
    debugEntry.cmdline = "debug loglevel=7 earlyprintk=vga";
    debugEntry.uuid = generateUuid();
    debugEntry.isDefault = false;
    debugEntry.isEnabled = true;
    debugEntry.timeout = 0;
    debugEntry.virtMode = "native";
    debugEntry.bootCount = 0;
    m_entries.append(debugEntry);
    
    // Memory Test
    BootEntry memtestEntry;
    memtestEntry.id = "memtest86";
    memtestEntry.name = "Memory Test (MemTest86)";
    memtestEntry.kernelPath = "/boot/memtest86.bin";
    memtestEntry.initrdPath = "";
    memtestEntry.cmdline = "";
    memtestEntry.uuid = generateUuid();
    memtestEntry.isDefault = false;
    memtestEntry.isEnabled = true;
    memtestEntry.timeout = 0;
    memtestEntry.virtMode = "native";
    memtestEntry.bootCount = 0;
    m_entries.append(memtestEntry);
}

QString BootConfigModel::generateUuid() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString BootConfigModel::findInitrdForKernel(const QString &kernelPath) const
{
    QFileInfo kernelInfo(kernelPath);
    QString kernelName = kernelInfo.baseName();
    
    // Try common initrd naming patterns
    QStringList patterns;
    patterns << kernelName + ".img"
             << kernelName + ".initrd"
             << kernelName + ".initramfs"
             << "initrd.img-" + kernelName
             << "initramfs-" + kernelName + ".img";
    
    QDir dir = kernelInfo.dir();
    for (const QString &pattern : patterns) {
        if (dir.exists(pattern)) {
            return dir.filePath(pattern);
        }
    }
    
    return QString();
}
