#pragma once

#include <QDialog>
#include <QString>

class QTabWidget;

namespace chromarchy {

class HelpDialog final : public QDialog {
  Q_OBJECT

public:
  enum class Page {
    Overview = 0,
    Shortcuts,
    FormatsAndLimits,
    Diagnostics,
    Licenses,
    About,
  };

  static constexpr int pageCount = 6;
  static constexpr qsizetype maximumPageTextCharacters = 8192;
  static constexpr qsizetype maximumCombinedTextCharacters =
      pageCount * maximumPageTextCharacters;

  explicit HelpDialog(Page initialPage = Page::Overview,
                      QWidget* parent = nullptr);

private:
  void addPage(const QString& objectName, const QString& title,
               const QString& accessibleDescription, const QString& text);

  QTabWidget* tabs_ = nullptr;  // Owned by QObject parent.
};

}  // namespace chromarchy
