#include <QTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include "FileSystem.h"
#include "minecraft/GlobalOptions.h"

class GlobalOptionsTest : public QObject {
    Q_OBJECT

    static void put(const QString& path, const QByteArray& data)
    {
        QVERIFY(FS::ensureFolderPathExists(QFileInfo(path).absolutePath()));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    static QByteArray get(const QString& path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<missing>");
    }
    static GlobalOptions::Target target(const QString& root, const QString& mc, const QString& loader)
    {
        QDir d(root);
        return { d.filePath("game"), d.filePath("game/mods"), d.filePath("game/resourcepacks"), d.filePath("game/shaderpacks"), mc, loader, "inst" };
    }

   private slots:
    void test_captureApplyRestore()
    {
        QTemporaryDir tmp;
        const QDir root(tmp.path());
        const auto store = root.filePath("store");
        const auto src = target(root.filePath("a"), "1.21.1", "fabric");
        const auto same = target(root.filePath("b"), "1.21.1", "fabric");
        const auto other = target(root.filePath("c"), "1.20.1", "forge");

        put(src.modsDir + "/sodium.jar", "sodium");
        put(src.modsDir + "/off.jar.disabled", "off");
        put(src.resourcePacksDir + "/pack.zip", "pack");
        put(src.shaderPacksDir + "/shader.zip", "shader");
        put(src.gameRoot + "/options.txt", "fov:1.0\nresourcePacks:[\"vanilla\",\"file/pack.zip\"]\n");
        put(src.gameRoot + "/config/iris.properties", "shaderPack=shader.zip\nenableShaders=true\n");

        QVERIFY(!GlobalOptions::hasStore(store));
        QVERIFY(GlobalOptions::capture(src, store).ok);
        QVERIFY(GlobalOptions::hasStore(store));

        for (const auto& dst : { same, other }) {
            put(dst.modsDir + "/old.jar", "old");
            put(dst.resourcePacksDir + "/stale.zip", "stale");
            put(dst.gameRoot + "/options.txt", "fov:2.0\r\nresourcePacks:[\"vanilla\"]\r\nlang:en_us\r\n");
        }
        const auto backups = root.filePath("backups");

        auto r = GlobalOptions::apply(store, same, backups);
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(r.warnings.isEmpty());
        QCOMPARE(get(same.modsDir + "/sodium.jar"), QByteArray("sodium"));
        QCOMPARE(get(same.modsDir + "/off.jar.disabled"), QByteArray("off"));
        QCOMPARE(get(same.modsDir + "/old.jar"), QByteArray("<missing>"));
        QCOMPARE(get(same.resourcePacksDir + "/pack.zip"), QByteArray("pack"));
        QCOMPARE(get(same.shaderPacksDir + "/shader.zip"), QByteArray("shader"));
        QCOMPARE(get(same.gameRoot + "/options.txt"),
                 QByteArray("fov:2.0\r\nresourcePacks:[\"vanilla\",\"file/pack.zip\"]\r\nlang:en_us\r\n"));
        const auto iris = get(same.gameRoot + "/config/iris.properties");
        QVERIFY(iris.contains("shaderPack=shader.zip\n") && iris.contains("enableShaders=true\n"));

        // different version/loader: mods are skipped and untouched, everything else applies
        r = GlobalOptions::apply(store, other, backups);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.warnings.size(), 1);
        QCOMPARE(get(other.modsDir + "/old.jar"), QByteArray("old"));
        QCOMPARE(get(other.modsDir + "/sodium.jar"), QByteArray("<missing>"));
        QCOMPARE(get(other.shaderPacksDir + "/shader.zip"), QByteArray("shader"));

        // restore the backup taken for "same" (the one that contains its old mod)
        QString sameBackup;
        for (const auto& n : GlobalOptions::backups(backups)) {
            if (QFileInfo::exists(backups + "/" + n + "/mods/old.jar"))
                sameBackup = n;
        }
        QVERIFY(!sameBackup.isEmpty());
        r = GlobalOptions::restoreBackup(backups, sameBackup, same);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(get(same.modsDir + "/old.jar"), QByteArray("old"));
        QCOMPARE(get(same.modsDir + "/sodium.jar"), QByteArray("<missing>"));
        QCOMPARE(get(same.gameRoot + "/options.txt"), QByteArray("fov:2.0\r\nresourcePacks:[\"vanilla\"]\r\nlang:en_us\r\n"));
        QCOMPARE(get(same.gameRoot + "/config/iris.properties"), QByteArray("<missing>"));
    }

    void test_previousStore()
    {
        QTemporaryDir tmp;
        const QDir root(tmp.path());
        const auto store = root.filePath("store");
        const auto a = target(root.filePath("a"), "1.21.1", "fabric");
        const auto b = target(root.filePath("b"), "1.20.1", "forge");
        put(a.shaderPacksDir + "/one.zip", "1");
        put(b.shaderPacksDir + "/two.zip", "2");
        QVERIFY(GlobalOptions::capture(a, store).ok);
        QVERIFY(GlobalOptions::capture(b, store).ok);
        QVERIFY(GlobalOptions::hasPreviousStore(store));
        QVERIFY(GlobalOptions::restorePreviousStore(store).ok);
        QCOMPARE(get(store + "/shaderpacks/one.zip"), QByteArray("1"));
        QVERIFY(!GlobalOptions::apply(root.filePath("missing"), a, root.filePath("bk")).ok);
    }
};

QTEST_GUILESS_MAIN(GlobalOptionsTest)

#include "GlobalOptions_test.moc"
