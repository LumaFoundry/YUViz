#include "debugManager.h"
#include <QDebug>

DebugManager& DebugManager::instance() {
    static DebugManager instance;
    return instance;
}

void DebugManager::initialize(const QString& debugFilters) {
    // Enable debug mode
    m_debugEnabled = true;

    if (debugFilters == "max") {
        m_allEnabled = true;
        return;
    }

    // If "min", only min components will be enabled
    if (debugFilters == "min") {
        return;
    }

    QStringList filters = debugFilters.split(':', Qt::SkipEmptyParts);
    for (const QString& filter : filters) {
        QString trimmedFilter = filter.trimmed();
        if (trimmedFilter.isEmpty())
            continue;

        // Directly use the input name
        m_enabledComponents.insert(trimmedFilter);
    }

    // Print enabled components
    if (!m_enabledComponents.isEmpty()) {
        qDebug() << "[DebugManager] Enabled debug components:" << m_enabledComponents.values();
    }
}

bool DebugManager::isEnabled(const QString& component) const {
    if (m_allEnabled)
        return true;

    // Check if component is marked as min
    if (m_minComponents.contains(component)) {
        return m_debugEnabled; // Only print if debug is enabled
    }

    return m_enabledComponents.contains(component);
}

void DebugManager::debug(const QString& component, const QString& message, bool isMin) {
    if (isMin) {
        m_minComponents.insert(component);
    }

    if (isEnabled(component)) {
        qDebug().noquote() << QString("[%1] %2").arg(component.toUpper()).arg(message);
    }
}

void DebugManager::debug(const QString& component, const char* message, bool isMin) {
    debug(component, QString::fromUtf8(message), isMin);
}

void DebugManager::warning(const QString& component, const QString& message) {
    // Warning always prints regardless of debug settings
    qWarning().noquote() << QString("[%1] %2").arg(component.toUpper()).arg(message);
}

void DebugManager::warning(const QString& component, const char* message) {
    // Warning always prints regardless of debug settings
    qWarning().noquote() << QString("[%1] %2").arg(component.toUpper()).arg(QString::fromUtf8(message));
}

QStringList DebugManager::getEnabledComponents() const {
    return m_enabledComponents.values();
}

void DebugManager::enableComponent(const QString& component) {
    m_enabledComponents.insert(component);
}

void DebugManager::disableComponent(const QString& component) {
    m_enabledComponents.remove(component);
}

void DebugManager::clearFilters() {
    m_enabledComponents.clear();
    m_allEnabled = false;
}
