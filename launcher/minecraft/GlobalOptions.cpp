// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "GlobalOptions.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "FileSystem.h"
#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "modplatform/ModIndex.h"

namespace GlobalOptions {

QString storePath()
{
    return QDir(APPLICATION->dataRoot()).filePath("globaloptions/current");
}

QString backupRoot(const MinecraftInstance* inst)
{
    return QDir(APPLICATION->dataRoot()).filePath("globaloptions/backups/" + inst->id());
}

Target targetFor(MinecraftInstance* inst)
{
    QStringList loaders;
    for (auto loader : inst->getPackProfile()->getModLoadersList())
        loaders << ModPlatform::getModLoaderAsString(loader).toLower();
    loaders.sort();
    return { inst->gameRoot(),
             inst->modsRoot(),
             inst->resourcePacksDir(),
             inst->shaderPacksDir(),
             inst->getPackProfile()->getComponentVersion("net.minecraft"),
             loaders.join(','),
             inst->name() };
}

namespace {

constexpr int kMaxBackups = 10;
const char kManifest[] = "manifest.json";
const char kBackupManifest[] = "backup.json";

struct DirSpec {
    const char* name;
    QString Target::* member;
};
const DirSpec kDirs[] = { { "mods", &Target::modsDir },
                          { "resourcepacks", &Target::resourcePacksDir },
                          { "shaderpacks", &Target::shaderPacksDir } };

// "Which pack is selected" lives in plain key/value files; only these keys are touched, the rest of each file is preserved.
struct SelectionFile {
    const char* path;
    QChar sep;
    QStringList keys;
};
const QList<SelectionFile>& selectionFiles()
{
    static const QList<SelectionFile> files = {
        { "options.txt", ':', { "resourcePacks", "incompatibleResourcePacks" } },
        { "config/iris.properties", '=', { "shaderPack", "enableShaders" } },
        { "optionsshaders.txt", '=', { "shaderPack" } },
    };
    return files;
}

Result fail(const QString& error, const QStringList& warnings = {})
{
    return { false, error, warnings };
}

QJsonObject readJson(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool writeFile(const QString& path, const QByteArray& data)
{
    if (!FS::ensureFolderPathExists(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(data);
    return f.commit();
}

bool writeJson(const QString& path, const QJsonObject& obj)
{
    return writeFile(path, QJsonDocument(obj).toJson());
}

bool copyDir(const QString& src, const QString& dst)
{
    if (!FS::ensureFolderPathExists(dst))
        return false;
    if (!QFileInfo::exists(src))
        return true;
    FS::copy c(src, dst);
    c.copyDirectories(true);
    return c() && c.totalFailed() == 0;
}

bool clearDir(const QString& path)
{
    if (path.isEmpty())
        return false;
    if (QFileInfo::exists(path) && !FS::deleteContents(path))
        return false;
    return FS::ensureFolderPathExists(path);
}

bool removeTree(const QString& path)
{
    return !QFileInfo::exists(path) || FS::deletePath(path);
}

QString timestamp()
{
    return QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz");
}

// ---- selection files ------------------------------------------------------------------------------------------------

QString keyOf(const QString& line, QChar sep)
{
    return line.section(sep, 0, 0).trimmed();
}

QJsonObject readSelection(const QString& gameRoot)
{
    QJsonObject out;
    for (const auto& file : selectionFiles()) {
        QFile f(QDir(gameRoot).filePath(file.path));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        QJsonObject values;
        for (auto line : QString::fromUtf8(f.readAll()).split('\n')) {
            if (line.endsWith('\r'))
                line.chop(1);
            const auto key = keyOf(line, file.sep);
            if (file.keys.contains(key) && line.contains(file.sep))
                values[key] = line.section(file.sep, 1);
        }
        if (!values.isEmpty())
            out[file.path] = values;
    }
    return out;
}

bool writeSelection(const QString& gameRoot, const QJsonObject& selection)
{
    for (const auto& file : selectionFiles()) {
        const auto values = selection[file.path].toObject();
        if (values.isEmpty())
            continue;
        const auto path = QDir(gameRoot).filePath(file.path);
        QString text;
        bool existed = false;
        if (QFile f(path); f.open(QIODevice::ReadOnly)) {
            text = QString::fromUtf8(f.readAll());
            existed = true;
        }
        QStringList lines = text.split('\n');
        const bool hadTrailingNewline = existed && !lines.isEmpty() && lines.last().isEmpty();
        if (!existed || hadTrailingNewline)
            lines.removeLast();
        QStringList pending = values.keys();
        for (auto& line : lines) {
            const bool cr = line.endsWith('\r');
            const auto key = keyOf(line, file.sep);
            if (values.contains(key) && pending.contains(key)) {
                line = key + file.sep + values[key].toString() + (cr ? "\r" : "");
                pending.removeAll(key);
            }
        }
        for (const auto& key : pending)
            lines.append(key + file.sep + values[key].toString());
        if (!writeFile(path, (lines.join('\n') + '\n').toUtf8()))
            return false;
    }
    return true;
}

// ---- backups --------------------------------------------------------------------------------------------------------

void pruneBackups(const QString& backupRoot)
{
    const auto names = backups(backupRoot);
    for (int i = kMaxBackups; i < names.size(); i++)
        FS::deletePath(QDir(backupRoot).filePath(names[i]));
}

// Copies the given dirs and files of @a dst into a new backup folder. Returns its path, or empty with @a error set.
QString makeBackup(const Target& dst, const QString& backupRoot, const QStringList& dirNames, const QStringList& files, QString& error)
{
    const auto dir = QDir(backupRoot).filePath(timestamp());
    QJsonArray dirsJson, filesJson, missingJson;
    bool ok = FS::ensureFolderPathExists(dir);
    for (const auto& spec : kDirs) {
        if (!ok || !dirNames.contains(spec.name))
            continue;
        ok = copyDir(dst.*spec.member, QDir(dir).filePath(spec.name));
        dirsJson.append(spec.name);
    }
    for (const auto& rel : files) {
        if (!ok)
            break;
        const auto src = QDir(dst.gameRoot).filePath(rel);
        if (!QFileInfo::exists(src)) {
            missingJson.append(rel);
            continue;
        }
        const auto target = QDir(dir).filePath("files/" + rel);
        ok = FS::ensureFolderPathExists(QFileInfo(target).absolutePath()) && QFile::copy(src, target);
        filesJson.append(rel);
    }
    if (ok)
        ok = writeJson(QDir(dir).filePath(kBackupManifest),
                       { { "dirs", dirsJson },
                         { "files", filesJson },
                         { "missing", missingJson },
                         { "mcVersion", dst.mcVersion },
                         { "loaders", dst.loaders },
                         { "created", QDateTime::currentDateTime().toString(Qt::ISODate) } });
    if (!ok) {
        FS::deletePath(dir);
        error = QObject::tr("Could not create a backup in %1.").arg(backupRoot);
        return {};
    }
    pruneBackups(backupRoot);
    return dir;
}

bool restoreFromBackupDir(const QString& dir, const Target& dst)
{
    const auto info = readJson(QDir(dir).filePath(kBackupManifest));
    if (info.isEmpty())
        return false;
    bool ok = true;
    const auto dirs = info["dirs"].toArray();
    for (const auto& spec : kDirs) {
        if (dirs.contains(QString(spec.name)))
            ok = clearDir(dst.*spec.member) && copyDir(QDir(dir).filePath(spec.name), dst.*spec.member) && ok;
    }
    for (const auto& rel : info["files"].toArray()) {
        const auto target = QDir(dst.gameRoot).filePath(rel.toString());
        QFile::remove(target);
        ok = FS::ensureFolderPathExists(QFileInfo(target).absolutePath()) &&
             QFile::copy(QDir(dir).filePath("files/" + rel.toString()), target) && ok;
    }
    for (const auto& rel : info["missing"].toArray())
        QFile::remove(QDir(dst.gameRoot).filePath(rel.toString()));
    return ok;
}

}  // namespace

bool hasStore(const QString& store)
{
    return QFileInfo::exists(QDir(store).filePath(kManifest));
}

bool hasPreviousStore(const QString& store)
{
    return hasStore(store + ".previous");
}

QString describeStore(const QString& store)
{
    if (!hasStore(store))
        return {};
    const auto m = readJson(QDir(store).filePath(kManifest));
    const auto when = QDateTime::fromString(m["created"].toString(), Qt::ISODate);
    return QObject::tr("Saved from \"%1\" (Minecraft %2%3) on %4")
        .arg(m["name"].toString(), m["mcVersion"].toString(),
             m["loaders"].toString().isEmpty() ? QString() : ", " + m["loaders"].toString(), when.toString(Qt::TextDate));
}

Result capture(const Target& src, const QString& store)
{
    const auto tmp = store + ".tmp";
    const auto previous = store + ".previous";
    if (!removeTree(tmp) || !FS::ensureFolderPathExists(tmp))
        return fail(QObject::tr("Could not prepare %1.").arg(tmp));

    for (const auto& spec : kDirs) {
        if (src.*spec.member == QString() || !copyDir(src.*spec.member, QDir(tmp).filePath(spec.name))) {
            removeTree(tmp);
            return fail(QObject::tr("Could not copy the %1 folder.").arg(spec.name));
        }
    }
    if (!writeJson(QDir(tmp).filePath(kManifest),
                   { { "format", 1 },
                     { "name", src.name },
                     { "mcVersion", src.mcVersion },
                     { "loaders", src.loaders },
                     { "created", QDateTime::currentDateTime().toString(Qt::ISODate) },
                     { "selection", readSelection(src.gameRoot) } })) {
        removeTree(tmp);
        return fail(QObject::tr("Could not write the global options manifest."));
    }

    // swap in the new snapshot; the old one stays available as ".previous"
    const bool hadStore = QFileInfo::exists(store);
    if (hadStore && (!removeTree(previous) || !QDir().rename(store, previous))) {
        removeTree(tmp);
        return fail(QObject::tr("Could not back up the current global options."));
    }
    if (!QDir().rename(tmp, store)) {
        if (hadStore)
            QDir().rename(previous, store);
        removeTree(tmp);
        return fail(QObject::tr("Could not save the global options."));
    }
    return { true, {}, {} };
}

Result restorePreviousStore(const QString& store)
{
    const auto previous = store + ".previous";
    const auto swap = store + ".swap";
    if (!hasStore(previous))
        return fail(QObject::tr("There are no previous global options to restore."));
    if (!removeTree(swap))
        return fail(QObject::tr("Could not prepare %1.").arg(swap));
    const bool hadStore = QFileInfo::exists(store);
    if (hadStore && !QDir().rename(store, swap))
        return fail(QObject::tr("Could not move the current global options aside."));
    if (!QDir().rename(previous, store)) {
        if (hadStore)
            QDir().rename(swap, store);
        return fail(QObject::tr("Could not restore the previous global options."));
    }
    if (hadStore)
        QDir().rename(swap, previous);
    return { true, {}, {} };
}

Result apply(const QString& store, const Target& dst, const QString& backupRoot)
{
    if (!hasStore(store))
        return fail(QObject::tr("No global options have been saved yet. Use \"Copy to Global\" on an instance first."));
    const auto manifest = readJson(QDir(store).filePath(kManifest));

    QStringList warnings;
    const bool modsCompatible = !dst.loaders.isEmpty() && dst.mcVersion == manifest["mcVersion"].toString() &&
                                dst.loaders == manifest["loaders"].toString();
    QStringList dirNames;
    for (const auto& spec : kDirs) {
        if (dst.*spec.member == QString())
            continue;
        if (QString(spec.name) == "mods" && !modsCompatible) {
            warnings << QObject::tr("Mods skipped: the global mods are for Minecraft %1 (%2), this instance uses %3 (%4).")
                            .arg(manifest["mcVersion"].toString(), manifest["loaders"].toString().isEmpty() ? QObject::tr("no loader") : manifest["loaders"].toString(),
                                 dst.mcVersion, dst.loaders.isEmpty() ? QObject::tr("no loader") : dst.loaders);
            continue;
        }
        dirNames << spec.name;
    }
    const auto selection = manifest["selection"].toObject();
    QStringList files;
    for (const auto& file : selectionFiles()) {
        if (selection.contains(file.path))
            files << file.path;
    }

    QString error;
    const auto backup = makeBackup(dst, backupRoot, dirNames, files, error);
    if (backup.isEmpty())
        return fail(error, warnings);

    bool ok = true;
    for (const auto& spec : kDirs) {
        if (ok && dirNames.contains(spec.name))
            ok = clearDir(dst.*spec.member) && copyDir(QDir(store).filePath(spec.name), dst.*spec.member);
    }
    ok = ok && writeSelection(dst.gameRoot, selection);
    if (!ok) {
        const bool rolledBack = restoreFromBackupDir(backup, dst);
        return fail(rolledBack ? QObject::tr("Applying failed; the instance was restored to its previous state.")
                               : QObject::tr("Applying failed and the instance could not be fully restored. Its previous state is saved in %1.").arg(backup),
                    warnings);
    }
    return { true, {}, warnings };
}

QStringList backups(const QString& backupRoot)
{
    QStringList out;
    for (const auto& name : QDir(backupRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed)) {
        if (QFileInfo::exists(QDir(backupRoot).filePath(name + "/" + kBackupManifest)))
            out << name;
    }
    return out;
}

Result restoreBackup(const QString& backupRoot, const QString& name, const Target& dst)
{
    if (!backups(backupRoot).contains(name))
        return fail(QObject::tr("That backup no longer exists."));
    const auto dir = QDir(backupRoot).filePath(name);
    const auto info = readJson(QDir(dir).filePath(kBackupManifest));

    QStringList dirNames, files;
    for (const auto& d : info["dirs"].toArray())
        dirNames << d.toString();
    for (const auto& f : info["files"].toArray())
        files << f.toString();
    for (const auto& f : info["missing"].toArray())
        files << f.toString();

    // back up the present state first so the restore itself can be undone
    QString error;
    const auto undo = makeBackup(dst, backupRoot, dirNames, files, error);
    if (undo.isEmpty())
        return fail(error);
    if (!restoreFromBackupDir(dir, dst)) {
        const bool rolledBack = restoreFromBackupDir(undo, dst);
        return fail(rolledBack ? QObject::tr("Restoring failed; the instance was returned to its previous state.")
                               : QObject::tr("Restoring failed and the instance could not be fully returned to its previous state (saved in %1).").arg(undo));
    }
    return { true, {}, {} };
}

}  // namespace GlobalOptions
