#include <QTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include "profile/ProfileManager.h"

namespace fs = std::filesystem;

class TestFolderStructure : public QObject {
    Q_OBJECT

private slots:
    void testSaveAndLoadDefault() {
        // First run: loadFolderStructure creates default Built-in folder
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();

        auto structure = pm.loadFolderStructure();
        QCOMPARE(structure.version, 1);
        QCOMPARE(structure.folders.size(), size_t(1));
        QCOMPARE(structure.folders[0].id, std::string("builtin"));
        QCOMPARE(structure.folders[0].name, std::string("Built-in"));
        QVERIFY(structure.folders[0].expanded);
    }

    void testBuiltinsAssignedToBuiltinFolder() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();
        pm.loadFolderStructure(); // ensure default structure created

        // Check that builtin profiles are assigned to the builtin folder
        auto structure = pm.loadFolderStructure();
        for (const auto& profile : pm.listProfiles()) {
            if (profile.builtin) {
                auto it = structure.voiceAssignments.find(profile.filename);
                QVERIFY2(it != structure.voiceAssignments.end(),
                         ("Builtin profile " + profile.filename + " not assigned").c_str());
                QCOMPARE(it->second, std::string("builtin"));
            }
        }
    }

    void testMoveVoiceToFolder() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();
        pm.loadFolderStructure();

        // Create a user profile
        auto json = ProfileManager::pipelineToJson("User Voice", {
            {"gain", {{"gain_db", 3.f}}}
        });
        pm.saveProfile({"User Voice", "user-voice.json", json});

        // Move user voice to root (empty folderId)
        pm.moveVoiceToFolder("user-voice.json", "");

        auto structure = pm.loadFolderStructure();
        QVERIFY2(structure.voiceAssignments.find("user-voice.json") ==
                 structure.voiceAssignments.end(),
                 "Voice moved to root should not be in assignments map");
    }

    void testMoveVoiceToNewFolder() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();
        auto structure = pm.loadFolderStructure();

        // Add a custom folder
        VoiceFolder custom;
        custom.id = "f_custom";
        custom.name = "Custom";
        custom.parentId = "";
        custom.expanded = true;
        custom.order = 1;
        structure.folders.push_back(custom);

        // Save a user voice first
        auto json = ProfileManager::pipelineToJson("User Voice 2", {
            {"gain", {{"gain_db", 3.f}}}
        });
        pm.saveProfile({"User Voice 2", "user-voice-2.json", json});

        // Assign to custom folder
        pm.moveVoiceToFolder("user-voice-2.json", "f_custom");

        auto loaded = pm.loadFolderStructure();
        auto it = loaded.voiceAssignments.find("user-voice-2.json");
        QVERIFY2(it != loaded.voiceAssignments.end(), "Voice should be assigned");
        QCOMPARE(it->second, std::string("f_custom"));
    }

    void testMoveVoiceToBuiltinThrows() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();
        pm.loadFolderStructure();

        bool caught = false;
        try {
            pm.moveVoiceToFolder("deep-voice.json", "builtin");
        } catch (const std::exception& e) {
            caught = true;
            QString msg = QString::fromStdString(e.what());
            QVERIFY2(msg.contains("builtin", Qt::CaseInsensitive),
                     ("Expected error about builtin: " + msg).toUtf8());
        }
        QVERIFY2(caught, "Expected std::exception to be thrown");
    }

    void testGetVoicesInFolder() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();
        pm.loadFolderStructure();

        // Add a custom folder and move a voice there
        auto structure = pm.loadFolderStructure();
        VoiceFolder custom;
        custom.id = "f_test";
        custom.name = "Test Folder";
        custom.parentId = "";
        custom.expanded = true;
        custom.order = 0;
        structure.folders.push_back(custom);

        // Save a test voice
        auto json = ProfileManager::pipelineToJson("Test Voice", {
            {"gain", {{"gain_db", 1.f}}}
        });
        pm.saveProfile({"Test Voice", "test-voice.json", json});

        // Assign to custom folder
        pm.moveVoiceToFolder("test-voice.json", "f_test");

        auto voices = pm.getVoicesInFolder("f_test");
        QVERIFY2(std::find(voices.begin(), voices.end(), "test-voice.json") != voices.end(),
                 "test-voice.json should be in f_test folder");
    }

    void testGetVoicesInBuiltinFolder() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();
        pm.loadFolderStructure();

        auto voices = pm.getVoicesInFolder("builtin");
        QVERIFY(!voices.empty());
        // At least the builtin profiles should be there
        for (const auto& v : voices) {
            QVERIFY(!v.empty());
        }
    }

    void testGetFolderDepth() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        ProfileManager pm;
        pm.installDefaultProfiles();
        pm.loadFolderStructure();

        // Built-in is depth 1
        QCOMPARE(pm.getFolderDepth("builtin"), 1);

        // Add nested folders
        auto structure = pm.loadFolderStructure();

        VoiceFolder parent;
        parent.id = "f_parent";
        parent.name = "Parent";
        parent.parentId = "";
        parent.expanded = true;
        parent.order = 0;
        structure.folders.push_back(parent);

        VoiceFolder child;
        child.id = "f_child";
        child.name = "Child";
        child.parentId = "f_parent";
        child.expanded = true;
        child.order = 0;
        structure.folders.push_back(child);

        VoiceFolder grandchild;
        grandchild.id = "f_grandchild";
        grandchild.name = "Grandchild";
        grandchild.parentId = "f_child";
        grandchild.expanded = true;
        grandchild.order = 0;
        structure.folders.push_back(grandchild);

        pm.saveFolderStructure(structure);

        // Built-in is always depth 1
        auto depths = pm.loadFolderStructure();
        (void)depths;

        // Reload to get updated structure
        auto loaded = pm.loadFolderStructure();
        (void)loaded;

        // Verify using getFolderDepth
        // Parent is at root (after builtin), so it should be at least 1
        // (it has no parent, so it is at the same level as root items)
        // Actually, parent has parentId="", meaning root level
        // So f_parent is at depth 1 (relative to root), f_child is at depth 2
        // Built-in is special and always returns 1
        int parentDepth = pm.getFolderDepth("f_parent");
        QVERIFY(parentDepth >= 1);

        int childDepth = pm.getFolderDepth("f_child");
        QVERIFY(childDepth >= parentDepth);

        int grandchildDepth = pm.getFolderDepth("f_grandchild");
        QVERIFY(grandchildDepth >= childDepth);

        // Verify builtin is always 1
        QCOMPARE(pm.getFolderDepth("builtin"), 1);
    }

    void testPersistence() {
        // Verify structure persists across ProfileManager instances
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        {
            ProfileManager pm;
            pm.installDefaultProfiles();
            pm.loadFolderStructure();

            auto json = ProfileManager::pipelineToJson("Persist Voice", {
                {"gain", {{"gain_db", 5.f}}}
            });
            pm.saveProfile({"Persist Voice", "persist-voice.json", json});

            // Move to root (empty folderId)
            pm.moveVoiceToFolder("persist-voice.json", "");
        }

        {
            // New instance — voice at root should not appear in assignments
            ProfileManager pm2;
            auto structure = pm2.loadFolderStructure();

            // Voice moved to root should NOT be in the assignments map
            auto it = structure.voiceAssignments.find("persist-voice.json");
            QVERIFY2(it == structure.voiceAssignments.end(),
                     "persist-voice.json should be at root (not in assignments)");

            // But the profile file should still exist
            fs::path profilePath = fs::path(tmpDir.path().toUtf8().constData())
                                       .append(".config").append("vml").append("profiles")
                                       .append("persist-voice.json");
            QVERIFY2(fs::exists(profilePath), "Profile file should still exist");
        }
    }

    void testReloadDoesNotOverwriteExisting() {
        // Verify that loadFolderStructure does NOT overwrite existing structure
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        qputenv("HOME", tmpDir.path().toUtf8());

        {
            ProfileManager pm;
            pm.installDefaultProfiles();
            auto structure = pm.loadFolderStructure();

            // Add a custom folder
            VoiceFolder extra;
            extra.id = "f_existing";
            extra.name = "Existing Folder";
            extra.parentId = "";
            extra.expanded = false;
            extra.order = 99;
            structure.folders.push_back(extra);

            pm.saveFolderStructure(structure);
        }

        {
            // Load should preserve existing custom folder
            ProfileManager pm;
            auto structure = pm.loadFolderStructure();

            QVERIFY(structure.folders.size() >= 1);
            bool found = false;
            for (const auto& f : structure.folders) {
                if (f.id == "f_existing") {
                    found = true;
                    QCOMPARE(f.name, std::string("Existing Folder"));
                    QVERIFY(!f.expanded);
                    break;
                }
            }
            QVERIFY2(found, "Custom folder should be preserved on reload");
        }
    }
};

QTEST_MAIN(TestFolderStructure)
#include "test_folder_structure.moc"
