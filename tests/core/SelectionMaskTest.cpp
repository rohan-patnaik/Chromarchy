#include "core/SelectionMask.h"

#include <QTest>

using chromarchy::SelectionMask;

class SelectionMaskTest final : public QObject {
  Q_OBJECT

private slots:
  void rectangleCrossesSparseTileBoundaries();
  void selectAllAndInvertRemainSparse();
  void copiesDetachOnMutation();
};

void SelectionMaskTest::rectangleCrossesSparseTileBoundaries() {
  SelectionMask selection(QSize(1000, 800));
  selection.selectRectangle(QRect(250, 250, 20, 20));
  QCOMPARE(selection.allocatedTileCount(), 4);
  QCOMPARE(selection.coverage(QPoint(250, 250)), 255);
  QCOMPARE(selection.coverage(QPoint(269, 269)), 255);
  QCOMPARE(selection.coverage(QPoint(249, 249)), 0);
  QVERIFY(selection.dirtyRegion().contains(QPoint(250, 250)));
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

QTEST_APPLESS_MAIN(SelectionMaskTest)

#include "SelectionMaskTest.moc"
