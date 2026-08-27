#include "core/SelectionMask.h"

#include <QTest>

using chromarchy::SelectionMask;

class SelectionMaskTest final : public QObject {
  Q_OBJECT

private slots:
  void rectangleCrossesSparseTileBoundaries();
  void identicalRectangleReportsNoChange();
  void selectAllAndInvertRemainSparse();
  void copiesDetachOnMutation();
  void elidesTilesWhenCoverageReturnsToBase();
};

void SelectionMaskTest::rectangleCrossesSparseTileBoundaries() {
  SelectionMask selection(QSize(1000, 800));
  QVERIFY(selection.selectRectangle(QRect(250, 250, 20, 20)));
  QCOMPARE(selection.allocatedTileCount(), 4);
  QCOMPARE(selection.coverage(QPoint(250, 250)), 255);
  QCOMPARE(selection.coverage(QPoint(269, 269)), 255);
  QCOMPARE(selection.coverage(QPoint(249, 249)), 0);
  QVERIFY(selection.dirtyRegion().contains(QPoint(250, 250)));
}

void SelectionMaskTest::identicalRectangleReportsNoChange() {
  SelectionMask selection(QSize(512, 512));
  const QRect rectangle(100, 110, 200, 210);
  QVERIFY(selection.selectRectangle(rectangle));
  selection.takeDirtyRegion();

  QVERIFY(!selection.selectRectangle(rectangle));
  QVERIFY(selection.dirtyRegion().isEmpty());
}

void SelectionMaskTest::selectAllAndInvertRemainSparse() {
  SelectionMask selection(QSize(300'000, 300'000));
  selection.selectAll();
  QCOMPARE(selection.allocatedTileCount(), 0);
  QCOMPARE(selection.coverage(QPoint(299'999, 299'999)), 255);
  selection.invert();
  QCOMPARE(selection.allocatedTileCount(), 0);
  QVERIFY(selection.isEmpty());
}

void SelectionMaskTest::copiesDetachOnMutation() {
  SelectionMask original(QSize(512, 512));
  QVERIFY(original.setCoverage(QPoint(10, 10), 128));
  original.takeDirtyRegion();
  auto copy = original;
  QVERIFY(copy.setCoverage(QPoint(10, 10), 200));
  QCOMPARE(original.coverage(QPoint(10, 10)), 128);
  QCOMPARE(copy.coverage(QPoint(10, 10)), 200);
  QVERIFY(original.dirtyRegion().isEmpty());
}

void SelectionMaskTest::elidesTilesWhenCoverageReturnsToBase() {
  SelectionMask emptyBase(QSize(512, 512));
  QVERIFY(emptyBase.setCoverage(QPoint(10, 10), 128));
  auto retainedCopy = emptyBase;
  QVERIFY(emptyBase.setCoverage(QPoint(10, 10), 0));
  QCOMPARE(emptyBase.allocatedTileCount(), 0);
  QVERIFY(emptyBase.isEmpty());
  QCOMPARE(retainedCopy.allocatedTileCount(), 1);
  QCOMPARE(retainedCopy.coverage(QPoint(10, 10)), 128);

  SelectionMask fullBase(QSize(512, 512));
  QVERIFY(fullBase.selectAll());
  QVERIFY(fullBase.setCoverage(QPoint(300, 300), 64));
  QCOMPARE(fullBase.allocatedTileCount(), 1);
  QVERIFY(fullBase.setCoverage(QPoint(300, 300), 255));
  QCOMPARE(fullBase.allocatedTileCount(), 0);
  QCOMPARE(fullBase.coverage(QPoint(300, 300)), 255);

  const QRect rectangle(250, 250, 20, 20);
  QVERIFY(emptyBase.selectRectangle(rectangle));
  QCOMPARE(emptyBase.allocatedTileCount(), 4);
  QVERIFY(emptyBase.selectRectangle(rectangle, 0, false));
  QCOMPARE(emptyBase.allocatedTileCount(), 0);
  QVERIFY(emptyBase.isEmpty());
}

QTEST_APPLESS_MAIN(SelectionMaskTest)

#include "SelectionMaskTest.moc"
