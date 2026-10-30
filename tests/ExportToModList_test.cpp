// SPDX-License-Identifier: GPL-3.0-only
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "minecraft/mod/ResourcePack.h"
#include "minecraft/mod/ShaderPack.h"
#include "modplatform/helpers/ExportToModList.h"

class ExportToModListTest : public QObject {
    Q_OBJECT

   private slots:
    void packsAndFormats()
    {
        ShaderPack shader{QFileInfo("shaders.zip")};
        ResourcePack pack{QFileInfo("resources.zip")};
        Metadata::ModStruct shaderMeta;
        shaderMeta.name = "Shader & Pack";
        shaderMeta.versionNumber = "1.2";
        shader.setMetadata(shaderMeta);
        Metadata::ModStruct packMeta;
        packMeta.name = "Resource Pack";
        packMeta.versionNumber = "2.0";
        pack.setMetadata(packMeta);
        const QList<Resource*> resources{&shader, &pack};
        using namespace ExportToModList;

        QCOMPARE(exportToModList(resources, COMMA_LIST, None), QString("Shader & Pack, Resource Pack"));
        QCOMPARE(exportToModList({&pack}, COMMA_LIST, None), QString("Resource Pack"));
        QCOMPARE(exportToModList({}, COMMA_LIST, None), QString());
        QCOMPARE(exportToModList(resources, PLAINTXT, None), QString("Shader & Pack\nResource Pack"));
        QCOMPARE(exportToModList(resources, MARKDOWN, None), QString("- Shader & Pack\n- Resource Pack"));
        QCOMPARE(exportToModList(resources, CSV, None), QString("Shader & Pack\nResource Pack"));
        QVERIFY(exportToModList(resources, HTML, None).contains("<li>Shader &amp; Pack</li>"));
        QCOMPARE(exportToModList({&shader}, COMMA_LIST, OptionalData{ExportToModList::Version} | Authors | FileName),
                 QString("Shader & Pack [1.2] (shaders.zip)"));
        QCOMPARE(exportToModList({&shader}, "{name}|{mod_id}|{authors}|{version}|{filename}"),
                 QString("Shader & Pack|||1.2|shaders.zip"));
        const auto json = QJsonDocument::fromJson(exportToModList(resources, JSON, OptionalData{ExportToModList::Version} | Authors | FileName).toUtf8()).array();
        QCOMPARE(json.size(), 2);
        QCOMPARE(json[0].toObject()["name"].toString(), QString("Shader & Pack"));
        QCOMPARE(json[0].toObject()["version"].toString(), QString("1.2"));
        QVERIFY(!json[0].toObject().contains("authors"));
    }

    void modsKeepMetadata()
    {
        Mod mod{QFileInfo("mod.jar")};
        ModDetails details;
        details.name = "Example";
        details.mod_id = "example";
        details.version = "3.0";
        details.authors = {"Alice", "Bob"};
        details.homeurl = "https://example.org";
        mod.setDetails(details);
        using namespace ExportToModList;
        QCOMPARE(exportToModList({&mod}, PLAINTXT, OptionalData{ExportToModList::Version} | Authors | Url | FileName),
                 QString("Example (https://example.org) [3.0] by Alice, Bob (mod.jar)"));
        QCOMPARE(exportToModList({&mod}, "{name}|{mod_id}|{authors}"), QString("Example|example|Alice, Bob"));
        QVERIFY(exportToModList({&mod}, HTML, Authors).contains("by Alice, Bob"));
        const auto json = QJsonDocument::fromJson(exportToModList({&mod}, JSON, Authors).toUtf8()).array();
        QCOMPARE(json[0].toObject()["authors"].toArray().size(), 2);
    }
};

QTEST_GUILESS_MAIN(ExportToModListTest)
#include "ExportToModList_test.moc"
