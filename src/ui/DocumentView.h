#pragma once

#include "core/Document.h"
#include "core/DocumentHistory.h"
#include "core/NativeDocumentCodec.h"

#include <QWidget>

#include <functional>

namespace chromarchy {

class CanvasWidget;

class DocumentView final : public QWidget {
  Q_OBJECT

public:
  DocumentView(Document document, QString displayName, QString filePath,
               bool modified, QWidget* parent = nullptr);

  [[nodiscard]] Document& document() noexcept;
  [[nodiscard]] const Document& document() const noexcept;
  [[nodiscard]] CanvasWidget* canvas() const noexcept;
  [[nodiscard]] const QString& filePath() const noexcept;
  [[nodiscard]] const QString& displayName() const noexcept;
  [[nodiscard]] bool isModified() const noexcept;
  void setModified(bool modified);
  bool performCommand(const QString& description,
                      const std::function<bool(Document&)>& mutation);
  bool undo();
  bool redo();
  [[nodiscard]] const DocumentHistory& history() const noexcept;
  [[nodiscard]] QString tabTitle() const;
  [[nodiscard]] NativeDocumentWriteResult save(const QString& filePath);

signals:
  void titleChanged(const QString& title);
  void historyChanged();

private:
  Document document_;
  DocumentHistory history_;
  CanvasWidget* canvas_ = nullptr;  // Owned by QObject parent.
  QString displayName_;
  QString filePath_;
  bool modified_ = false;
};

}  // namespace chromarchy
