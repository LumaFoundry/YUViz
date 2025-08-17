#include <QtTest>
#include "utils/debugManager.h"

class DebugManagerTest : public QObject {
    Q_OBJECT
  private slots:
    void testSingleton();
    void testEnableDisableComponent();
    void testInitializeAndIsEnabled();
    void testDebugAndWarning();
    void testClearFilters();
};

void DebugManagerTest::testSingleton() {
    DebugManager& inst1 = DebugManager::instance();
    DebugManager& inst2 = DebugManager::instance();
    QVERIFY(&inst1 == &inst2); // Should be the same instance
}

void DebugManagerTest::testEnableDisableComponent() {
    DebugManager& mgr = DebugManager::instance();
    mgr.clearFilters();
    mgr.enableComponent("foo");
    QVERIFY(mgr.isEnabled("foo"));
    mgr.disableComponent("foo");
    QVERIFY(!mgr.isEnabled("foo"));
}

void DebugManagerTest::testInitializeAndIsEnabled() {
    DebugManager& mgr = DebugManager::instance();
    mgr.clearFilters();
    mgr.initialize("foo:bar");
    QVERIFY(mgr.isEnabled("foo"));
    QVERIFY(mgr.isEnabled("bar"));
    QVERIFY(!mgr.isEnabled("baz"));
}

void DebugManagerTest::testDebugAndWarning() {
    DebugManager& mgr = DebugManager::instance();
    mgr.clearFilters();
    mgr.enableComponent("foo");
    // These should not crash, but output goes to qDebug/qWarning
    mgr.debug("foo", "test message");
    mgr.debug("foo", "test message", true);
    mgr.debug("foo", "test message");
    mgr.warning("foo", "warning message");
    mgr.warning("foo", "warning message");
}

void DebugManagerTest::testClearFilters() {
    DebugManager& mgr = DebugManager::instance();
    mgr.enableComponent("foo");
    mgr.clearFilters();
    QVERIFY(!mgr.isEnabled("FOO"));
}

QTEST_MAIN(DebugManagerTest)
#include "test_debugmanager.moc"
