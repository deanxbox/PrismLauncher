// SPDX-License-Identifier: GPL-3.0-only
#include <QCheckBox>
#include <QClipboard>
#include <QDateEdit>
#include <QFile>
#include <QImageReader>
#include <QListView>
#include <QPersistentModelIndex>
#include <QTemporaryDir>
#include <QTest>

#include "ui/pages/instance/ScreenshotsPage.h"

class ScreenshotsPageTest : public QObject {
    Q_OBJECT

   private slots:
    void formatsAndActions_data()
    {
        QTest::addColumn<bool>("includeAvif");
        QTest::newRow("png-gif-webp") << false;
        QTest::newRow("with-avif") << true;
    }

    void formatsAndActions()
    {
        QFETCH(bool, includeAvif);
        if (includeAvif && !QImageReader::supportedImageFormats().contains("avif")) {
            QSKIP("AVIF runtime plugin is not installed");
        }
        QVERIFY(QImageReader::supportedImageFormats().contains("webp"));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage image(16, 16, QImage::Format_RGB32);
        image.fill(Qt::red);
        for (const auto& name : { "still.png", "shot.webp", "upper.WEBP", "photo.jpg", "upperjpeg.JPEG" }) {
            QVERIFY(image.save(dir.filePath(name)));
        }
        if (includeAvif) {
            QVERIFY(image.save(dir.filePath("avif.avif")));
            QVERIFY(image.save(dir.filePath("upper-avif.AVIF")));
        } else if (!QImageReader::supportedImageFormats().contains("avif")) {
            QFile unsupported(dir.filePath("unsupported.avif"));
            QVERIFY(unsupported.open(QIODevice::WriteOnly));
            unsupported.close();
        }
        QFile gif(dir.filePath("animation.gif"));
        QVERIFY(gif.open(QIODevice::WriteOnly));
        const auto gifData = QByteArray::fromBase64("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7");
        QCOMPARE(gif.write(gifData), gifData.size());
        gif.close();
        QFile broken(dir.filePath("broken.jpg"));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not an image");
        broken.close();
        QFile ignored(dir.filePath("notes.txt"));
        QVERIFY(ignored.open(QIODevice::WriteOnly));
        ignored.close();

        ScreenshotsPage page(dir.path());
        page.openedImpl();
        auto* view = page.findChild<QListView*>("listView");
        QVERIFY(view);
        auto* model = view->model();
        QVERIFY(model);
        const int count = includeAvif ? 9 : 7;
        QTRY_COMPARE(model->rowCount(view->rootIndex()), count);
        QStringList labels;
        for (int row = 0; row < model->rowCount(view->rootIndex()); ++row) {
            labels << model->index(row, 0, view->rootIndex()).data().toString();
        }
        labels.sort();
        QStringList expected{ "animation", "broken", "photo", "shot", "still", "upper", "upperjpeg" };
        if (includeAvif) {
            expected << "avif" << "upper-avif";
            expected.sort();
        }
        QCOMPARE(labels, expected);

        // Exercise real thumbnail generation and copy-image for new and existing formats.
        for (const auto& label : labels) {
            if (label == "broken") {
                continue; // must be listed without crashing; there is no image to render
            }
            QPersistentModelIndex index;
            for (int row = 0; row < model->rowCount(view->rootIndex()); ++row) {
                const auto candidate = model->index(row, 0, view->rootIndex());
                if (candidate.data().toString() == label) {
                    index = candidate;
                    break;
                }
            }
            QVERIFY(index.isValid());
            QTRY_VERIFY(!qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).isNull());
            if (label != "animation") {
                auto hasRedThumbnail = [&] {
                    const auto thumb = qvariant_cast<QIcon>(index.data(Qt::DecorationRole)).pixmap(256, 256).toImage();
                    if (thumb.isNull()) {
                        return false;
                    }
                    const auto color = thumb.pixelColor(thumb.width() / 2, thumb.height() / 2);
                    return color.red() > 240 && color.green() < 20 && color.blue() < 20;
                };
                QTRY_VERIFY(hasRedThumbnail());
            }
            view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
            QApplication::clipboard()->clear();
            QVERIFY(QMetaObject::invokeMethod(&page, "on_actionCopy_Image_triggered", Qt::DirectConnection));
            QVERIFY(!QApplication::clipboard()->image().isNull());
            if (label == "shot") {
                QVERIFY(model->setData(index, "renamed"));
                QTRY_VERIFY(QFile::exists(dir.filePath("renamed.webp")));
                QVERIFY(!QFile::exists(dir.filePath("shot.webp")));
            } else if (label == "avif") {
                QVERIFY(model->setData(index, "renamed-avif"));
                QTRY_VERIFY(QFile::exists(dir.filePath("renamed-avif.avif")));
                QVERIFY(!QFile::exists(dir.filePath("avif.avif")));
            }
        }

        // Existing date filtering still applies to every screenshot format.
        auto boxes = page.findChildren<QCheckBox*>();
        auto dates = page.findChildren<QDateEdit*>();
        QCOMPARE(boxes.size(), 2);
        QCOMPARE(dates.size(), 2);
        dates.first()->setDate(QDate::currentDate().addDays(1));
        boxes.first()->setChecked(true);
        QTRY_COMPARE(model->rowCount(view->rootIndex()), 0);
        boxes.first()->setChecked(false);
        QTRY_COMPARE(model->rowCount(view->rootIndex()), count);
    }
};

QTEST_MAIN(ScreenshotsPageTest)
#include "ScreenshotsPage_test.moc"
