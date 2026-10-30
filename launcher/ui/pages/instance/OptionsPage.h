// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include "minecraft/MinecraftInstance.h"
#include "ui/pages/BasePage.h"

class QLabel;
class QPushButton;

/** Instance page for copying this instance's mods / resource packs / shader packs to and from the global options. */
class OptionsPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit OptionsPage(MinecraftInstance* inst, QWidget* parent = nullptr);

    QString displayName() const override { return tr("Options"); }
    QIcon icon() const override
    {
        auto icon = QIcon::fromTheme("settings");
        return icon.isNull() ? QIcon::fromTheme("instance-settings") : icon;
    }
    QString id() const override { return "options"; }
    QString helpPage() const override { return "Instance-options"; }
    void openedImpl() override { refresh(); }

   private:
    void refresh();
    bool confirm(const QString& text);
    void report(const QString& title, const QString& okText, const QString& error, const QStringList& warnings);
    void copyToGlobal();
    void undoCopyToGlobal();
    void sync();
    void restoreBackup();

    MinecraftInstance* m_inst;
    QLabel* m_status;
    QPushButton* m_sync;
    QPushButton* m_undo;
    QPushButton* m_restore;
};
