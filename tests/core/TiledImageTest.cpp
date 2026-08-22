#include "core/TiledImage.h"

#include <QTest>

using chromarchy::TiledImage;

class TiledImageTest final : public QObject {
  Q_OBJECT

private slots:
  void remainsSparseAcrossTileBoundaries();
  void copiesDetachOnMutation();
  void reportsAndClearsDirtyRegion();
  void rendersRequestedRegion();
  void elidesTileWhenLastPixelBecomesZero();
  void zeroTileElisionPreservesCopiesAndNonzeroPixels();
};

void TiledImageTest::remainsSparseAcrossTileBoundaries() {
  TiledImage image(QSize(1024, 1024));
  QCOMPARE(image.allocatedTileCount(), 0);
  QVERIFY(image.setPixelColor(QPoint(255, 255), Qt::red));
  QVERIFY(image.setPixelColor(QPoint(256, 256), Qt::green));
  QCOMPARE(image.allocatedTileCount(), 2);
  QCOMPARE(image.pixelColor(QPoint(255, 255)), QColor(Qt::red));
  QCOMPARE(image.pixelColor(QPoint(256, 256)), QColor(Qt::green));
  QCOMPARE(image.pixelColor(QPoint(900, 900)), QColor(Qt::transparent));
  QVERIFY(!image.setPixelColor(QPoint(-1, 0), Qt::blue));
}

void TiledImageTest::copiesDetachOnMutation() {
  TiledImage original(QSize(512, 512));
  QVERIFY(original.setPixelColor(QPoint(10, 10), Qt::red));
  original.takeDirtyRegion();

  auto copy = original;
  QVERIFY(copy.setPixelColor(QPoint(10, 10), Qt::blue));

  QCOMPARE(original.pixelColor(QPoint(10, 10)), QColor(Qt::red));
  QCOMPARE(copy.pixelColor(QPoint(10, 10)), QColor(Qt::blue));
  QVERIFY(original.dirtyRegion().isEmpty());
}

void TiledImageTest::reportsAndClearsDirtyRegion() {
  TiledImage image(QSize(512, 512));
  QVERIFY(image.setPixelColor(QPoint(4, 7), Qt::red));
  QVERIFY(image.setPixelColor(QPoint(300, 301), Qt::blue));
  QVERIFY(image.dirtyRegion().contains(QPoint(4, 7)));
  QVERIFY(image.dirtyRegion().contains(QPoint(300, 301)));

  const auto dirty = image.takeDirtyRegion();
  QVERIFY(dirty.contains(QPoint(4, 7)));
  QVERIFY(image.dirtyRegion().isEmpty());
}

void TiledImageTest::rendersRequestedRegion() {
  TiledImage image(QSize(512, 512));
  QVERIFY(image.setPixelColor(QPoint(260, 270), QColor(10, 20, 30, 255)));

  const auto rendered = image.render(QRect(250, 260, 20, 20));
  QCOMPARE(rendered.size(), QSize(20, 20));
  QCOMPARE(rendered.pixelColor(QPoint(10, 10)), QColor(10, 20, 30, 255));
}

void TiledImageTest::elidesTileWhenLastPixelBecomesZero() {
  TiledImage image(QSize(512, 512));
  const QPoint position(300, 301);
  QVERIFY(image.setPixelColor(position, QColor(10, 20, 30, 128)));
  QCOMPARE(image.allocatedTileCount(), 1);
  image.takeDirtyRegion();

  QVERIFY(image.setPixelColor(position, Qt::transparent));
  QCOMPARE(image.allocatedTileCount(), 0);
  QCOMPARE(image.pixelColor(position), QColor(Qt::transparent));
  QCOMPARE(image.dirtyRegion(), QRegion(QRect(position, QSize(1, 1))));
}

void TiledImageTest::zeroTileElisionPreservesCopiesAndNonzeroPixels() {
  TiledImage original(QSize(256, 256));
  QVERIFY(original.setPixelColor(QPoint(1, 1), Qt::red));
  QVERIFY(original.setPixelColor(QPoint(2, 2), Qt::blue));
  original.takeDirtyRegion();
  auto copy = original;

  QVERIFY(copy.setPixelColor(QPoint(1, 1), Qt::transparent));
  QCOMPARE(copy.allocatedTileCount(), 1);
  QCOMPARE(copy.pixelColor(QPoint(2, 2)), QColor(Qt::blue));
  QCOMPARE(original.pixelColor(QPoint(1, 1)), QColor(Qt::red));
  QCOMPARE(original.pixelColor(QPoint(2, 2)), QColor(Qt::blue));

  QVERIFY(copy.setPixelColor(QPoint(2, 2), Qt::transparent));
  QCOMPARE(copy.allocatedTileCount(), 0);
  QCOMPARE(original.allocatedTileCount(), 1);
}

QTEST_APPLESS_MAIN(TiledImageTest)

#include "TiledImageTest.moc"
