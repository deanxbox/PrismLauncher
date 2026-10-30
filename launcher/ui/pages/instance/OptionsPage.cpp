// SPDX-License-Identifier: GPL-3.0-only

#include "OptionsPage.h"

#include <QInputDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "minecraft/GlobalOptions.h"

OptionsPage::OptionsPage(MinecraftInstance* inst, QWidget* parent) : QWidget(parent), m_inst(inst)
{
    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        tr("Global options are a shared copy of the mods, resource packs and shader packs of one instance, including which resource "
           "pack and shader pack are selected. Mods are only applied to instances with the same Minecraft version and mod loader."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* copy = new QPushButton(tr("Copy to Global"), this);
    copy->setToolTip(tr("Replace the global options with this instance's mods, resource packs and shader packs."));
    m_undo = new QPushButton(tr("Undo Copy to Global"), this);
    m_undo->setToolTip(tr("Bring back the global options that were replaced by the last copy."));
    m_sync = new QPushButton(tr("Sync Global Options"), this);
    m_sync->setToolTip(tr("Replace this instance's mods, resource packs and shader packs with the global options."));
    m_restore = new QPushButton(tr("Restore Backup..."), this);
    m_restore->setToolTip(tr("Return this instance to its state before an earlier sync."));

    for (auto* b : { copy, m_undo, m_sync, m_restore }) {
        auto* row = new QHBoxLayout();
        row->addWidget(b);
        row->addStretch();
        layout->addLayout(row);
    }
    layout->addStretch();

    connect(copy, &QPushButton::clicked, this, &OptionsPage::copyToGlobal);
    connect(m_undo, &QPushButton::clicked, this, &OptionsPage::undoCopyToGlobal);
    connect(m_sync, &QPushButton::clicked, this, &OptionsPage::sync);
    connect(m_restore, &QPushButton::clicked, this, &OptionsPage::restoreBackup);
    refresh();
}

void OptionsPage::refresh()
{
    const auto store = GlobalOptions::storePath();
    const auto desc = GlobalOptions::describeStore(store);
    m_status->setText(desc.isEmpty() ? tr("No global options saved yet.") : desc);
    m_sync->setEnabled(!desc.isEmpty());
    m_undo->setEnabled(GlobalOptions::hasPreviousStore(store));
    m_restore->setEnabled(!GlobalOptions::backups(GlobalOptions::backupRoot(m_inst)).isEmpty());
}

bool OptionsPage::confirm(const QString& text)
{
    if (m_inst->isRunning()) {
        QMessageBox::warning(this, tr("Instance running"), tr("Close the game before changing its options."));
        return false;
    }
    return QMessageBox::question(this, tr("Are you sure?"), text) == QMessageBox::Yes;
}

void OptionsPage::report(const QString& title, const QString& okText, const QString& error, const QStringList& warnings)
{
    refresh();
    if (!error.isEmpty()) {
        QMessageBox::critical(this, title, (QStringList{ error } + warnings).join("\n\n"));
    } else if (!warnings.isEmpty()) {
        QMessageBox::warning(this, title, (QStringList{ okText } + warnings).join("\n\n"));
    } else {
        QMessageBox::information(this, title, okText);
    }
}

void OptionsPage::copyToGlobal()
{
    if (!confirm(tr("Replace the global options with the mods, resource packs and shader packs of this instance?\n"
                    "The previous global options can be brought back with \"Undo Copy to Global\".")))
        return;
    const auto r = GlobalOptions::capture(GlobalOptions::targetFor(m_inst), GlobalOptions::storePath());
    report(tr("Copy to Global"), tr("Global options updated."), r.error, r.warnings);
}

void OptionsPage::undoCopyToGlobal()
{
    if (!confirm(tr("Replace the global options with the ones that were saved before the last copy?")))
        return;
    const auto r = GlobalOptions::restorePreviousStore(GlobalOptions::storePath());
    report(tr("Undo Copy to Global"), tr("Previous global options restored."), r.error, r.warnings);
}

void OptionsPage::sync()
{
    if (!confirm(tr("Replace the mods, resource packs and shader packs of this instance with the global options?\n"
                    "A backup is made first and can be restored with \"Restore Backup...\".")))
        return;
    const auto r = GlobalOptions::apply(GlobalOptions::storePath(), GlobalOptions::targetFor(m_inst), GlobalOptions::backupRoot(m_inst));
    report(tr("Sync Global Options"), tr("This instance now matches the global options."), r.error, r.warnings);
}

void OptionsPage::restoreBackup()
{
    const auto root = GlobalOptions::backupRoot(m_inst);
    const auto names = GlobalOptions::backups(root);
    bool ok = false;
    const auto name = QInputDialog::getItem(this, tr("Restore Backup"), tr("Backup (newest first):"), names, 0, false, &ok);
    if (!ok || !confirm(tr("Restore this instance to the backup \"%1\"?\nIts current state is backed up first.").arg(name)))
        return;
    const auto r = GlobalOptions::restoreBackup(root, name, GlobalOptions::targetFor(m_inst));
    report(tr("Restore Backup"), tr("Backup restored."), r.error, r.warnings);
}
