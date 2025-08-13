#pragma once

#include <QDebug>
#include <QSet>
#include <QString>
#include <QStringList>

class DebugManager {
  public:
    static DebugManager& instance();

    // Initialize debug filters from command line
    void initialize(const QString& debugFilters);

    // Check if debug is enabled for a specific component
    bool isEnabled(const QString& component) const;

    // Debug output functions for different components
    void debug(const QString& component, const QString& message, bool isMin = false);
    void debug(const QString& component, const char* message, bool isMin = false);

    // Warning output functions that always print
    void warning(const QString& component, const QString& message);
    void warning(const QString& component, const char* message);

    // Get all enabled components
    QStringList getEnabledComponents() const;

    // Enable/disable specific components at runtime
    void enableComponent(const QString& component);
    void disableComponent(const QString& component);

    // Clear all filters (enable all debug output)
    void clearFilters();

  private:
    DebugManager() = default;

    QSet<QString> m_enabledComponents;
    QSet<QString> m_minComponents; // Components marked as min
    bool m_allEnabled = false;
    bool m_debugEnabled = false;
};

// Global debug and warning functions for better IntelliSense support
inline void debug(const QString& component, const QString& message, bool isMin = false) {
    DebugManager::instance().debug(component, message, isMin);
}

inline void debug(const QString& component, const char* message, bool isMin = false) {
    DebugManager::instance().debug(component, message, isMin);
}

inline void warning(const QString& component, const QString& message) {
    DebugManager::instance().warning(component, message);
}

inline void warning(const QString& component, const char* message) {
    DebugManager::instance().warning(component, message);
}
