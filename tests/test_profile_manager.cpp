#include <QTest>
#include <QTemporaryDir>
#include "profile/ProfileManager.h"

class TestProfileManager : public QObject {
    Q_OBJECT

private slots:
    void testSaveAndLoad() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;

        auto json = ProfileManager::pipelineToJson("Test", {
            {"gain", {{"gain_db", 6.f}}}
        });
        pm.saveProfile({"Test", "test.json", json});

        auto profiles = pm.listProfiles();
        QCOMPARE(profiles.size(), size_t(1));
        QCOMPARE(profiles[0].name, std::string("Test"));

        auto loaded = pm.loadProfile("test.json");
        QCOMPARE(loaded.name, std::string("Test"));

        auto pipeline = ProfileManager::jsonToPipeline(loaded.data);
        QCOMPARE(pipeline.size(), size_t(1));
        QCOMPARE(pipeline[0].first, std::string("gain"));
        QVERIFY(std::abs(pipeline[0].second[0].second - 6.f) < 0.01f);
    }

    void testDelete() {
        QTemporaryDir tmpDir;
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        auto json = ProfileManager::pipelineToJson("Del", {});
        pm.saveProfile({"Del", "del.json", json});
        QCOMPARE(pm.listProfiles().size(), size_t(1));

        pm.deleteProfile("del.json");
        QCOMPARE(pm.listProfiles().size(), size_t(0));
    }

    void testDefaultProfiles() {
        QTemporaryDir tmpDir;
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();
        auto profiles = pm.listProfiles();
        QCOMPARE(profiles.size(), size_t(3));
    }
};

QTEST_MAIN(TestProfileManager)
#include "test_profile_manager.moc"
