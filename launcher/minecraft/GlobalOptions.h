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

#pragma once

#include <QString>
#include <QStringList>

class MinecraftInstance;

/**
 * Global options: a snapshot of an instance's mods, resource packs, shader packs and the
 * matching "selected pack" settings, which can be applied to other instances.
 *
 * Store layout:   <store>/manifest.json, mods/, resourcepacks/, shaderpacks/
 * Backup layout:  <backupRoot>/<timestamp>/backup.json, <dirs...>, files/<relative path>
 */
namespace GlobalOptions {

/** Where the shared snapshot lives (inside the launcher data folder). */
QString storePath();
/** Where backups of one instance live (inside the launcher data folder, not the instance). */
QString backupRoot(const MinecraftInstance* inst);

/** The pieces of an instance this feature needs to know about. */
struct Target {
    QString gameRoot;
    QString modsDir;
    QString resourcePacksDir;
    QString shaderPacksDir;
    QString mcVersion;
    QString loaders;  // e.g. "fabric"; empty for vanilla
    QString name;     // display only
};

Target targetFor(MinecraftInstance* inst);

struct Result {
    bool ok = false;
    QString error;
    QStringList warnings;
};

bool hasStore(const QString& store);
/** One-line description of the stored snapshot, empty if there is none. */
QString describeStore(const QString& store);

/** Snapshots @a src into @a store. The previous store is kept as "<store>.previous". */
Result capture(const Target& src, const QString& store);
/** Swaps "<store>.previous" back in (and keeps the replaced store as the new ".previous"). */
Result restorePreviousStore(const QString& store);
bool hasPreviousStore(const QString& store);

/**
 * Mirrors @a store onto @a dst after backing up everything it will change into @a backupRoot.
 * Mods are skipped (and left untouched) when the Minecraft version or loader differs.
 * On failure the instance is rolled back from the backup.
 */
Result apply(const QString& store, const Target& dst, const QString& backupRoot);

/** Backup names, newest first. */
QStringList backups(const QString& backupRoot);
/** Restores a backup; the current state is backed up first so the restore can be undone. */
Result restoreBackup(const QString& backupRoot, const QString& name, const Target& dst);

}  // namespace GlobalOptions
