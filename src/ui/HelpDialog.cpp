#include "ui/HelpDialog.h"

#include "core/Document.h"
#include "core/ImageIO.h"
#include "core/NativeDocumentCodec.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextCursor>
#include <QVBoxLayout>

namespace chromarchy {
namespace {

QString overviewText() {
  return QStringLiteral(
      "Chromarchy Offline Help\n\n"
      "Chromarchy is an original offline-first raster image editor. Core "
      "document, layer, selection, view, native save, and raster import/export "
      "workflows run locally. No help page loads remote content, sends "
      "telemetry, or requires an account.\n\n"
      "The capability catalog is intentionally strict: Partial means a usable "
      "slice exists while named evidence remains incomplete. Planned and "
      "Blocked workflows are not claimed as implemented.\n\n"
      "Use the tabs in this dialog for keyboard shortcuts, supported "
      "format/resource limits, bounded runtime diagnostics, license text, and "
      "product identity.");
}

QString shortcutsText() {
  return QStringLiteral(
      "Keyboard Shortcuts\n\n"
      "File\n"
      "  Ctrl+N                 New document\n"
      "  Ctrl+O                 Open local file\n"
      "  Ctrl+S                 Save native document\n"
      "  Ctrl+Shift+S           Save As\n"
      "  Ctrl+W                 Close document\n"
      "  Ctrl+Q                 Resolve open documents and quit\n"
      "  Ctrl+Alt+1 … 9         Open recent entry 1 … 9\n"
      "  Ctrl+Alt+Shift+Delete  Clear recent paths\n\n"
      "Edit and selection\n"
      "  Ctrl+Z                 Undo\n"
      "  Ctrl+Shift+Z           Redo\n"
      "  Ctrl+A                 Select all\n"
      "  Ctrl+Shift+A           Deselect\n"
      "  Ctrl+Shift+I           Invert selection\n\n"
      "View\n"
      "  Ctrl++ / Ctrl+-        Zoom in / out\n"
      "  Ctrl+0                 Actual pixels\n"
      "  Ctrl+Shift+0           Fit canvas within current view\n"
      "  Arrow keys             Pan canvas by 32 viewport pixels\n"
      "  Shift+Arrow keys       Pan canvas by 128 viewport pixels\n"
      "  Ctrl+Alt+Right         Rotate view clockwise\n"
      "  Ctrl+Alt+Left          Rotate view counterclockwise\n"
      "  Ctrl+Alt+0             Reset view rotation\n"
      "  Ctrl+Alt+G             Toggle pixel grid (visible from 800%)\n\n"
      "Layers\n"
      "  Ctrl+Shift+N           New pixel layer\n"
      "  Ctrl+J                 Duplicate layer\n"
      "  F2                     Rename layer\n"
      "  Delete                 Remove layer\n"
      "  Ctrl+Shift+]           Move layer up\n"
      "  Ctrl+Shift+[           Move layer down\n"
      "  Ctrl+E                 Merge down\n\n"
      "Help\n"
      "  F1                     Open this offline help\n"
      "  Escape                 Close dialogs without applying changes");
}

QString formatsAndLimitsText() {
  return QStringLiteral(
             "Formats and Resource Limits\n\n"
             "Native documents\n"
             "  Extension: .chromarchy\n"
             "  Current writer: version 2; version 1 remains loadable\n"
             "  Atomic destination replacement with bounded file/tile payloads\n\n"
             "Raster import/export\n"
             "  Current Qt-codec surface: PNG, JPEG, TIFF, WebP, OpenEXR\n"
             "  Import file cap: %1 MiB\n"
             "  Full-frame decoded/export cap: %2 million pixels\n"
             "  Metadata is stripped by the current raster export policy\n\n"
             "Canvas and local state\n"
             "  Maximum canvas dimension: %3 pixels per axis\n"
             "  Sparse storage avoids allocating untouched full canvases\n"
             "  Recent paths: at most 20 entries and 4096 UTF-8 bytes each\n"
             "  View rotation: four ephemeral quarter-turn states\n\n"
             "Unsupported or corrupt inputs fail with an error; they are not "
             "silently reinterpreted as a supported format.")
      .arg(ImageIO::maximumImportFileBytes / (1024 * 1024))
      .arg(ImageIO::maximumImportPixels / 1'000'000)
      .arg(Document::maximumDimension);
}

QString diagnosticsText() {
  return QStringLiteral(
             "Bounded Runtime Diagnostics\n\n"
             "Application: Chromarchy %1\n"
             "Qt runtime: %2\n"
             "Qt platform plugin: %3\n"
             "Process word size: %4-bit\n"
             "Maximum canvas dimension: %5\n"
             "Native maximum file bytes: %6\n\n"
             "This page uses in-process constants and Qt runtime identifiers "
             "only. It does not execute shell commands, enumerate personal "
             "files, probe the network, or transmit diagnostics.")
      .arg(QApplication::applicationVersion().left(128),
           QString::fromLatin1(qVersion()).left(128),
           QApplication::platformName().left(128))
      .arg(sizeof(void*) * 8)
      .arg(Document::maximumDimension)
      .arg(NativeDocumentCodec::maximumNativeFileBytes);
}

QString licenseText() {
  return QStringLiteral("Chromarchy License\n\n")
      + QStringLiteral(R"LICENSE(MIT License

Copyright (c) 2026 Rohan Patnaik

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
)LICENSE")
      + QStringLiteral(
          "\nQt and optional system codecs retain their own licenses. This "
          "dialog does not claim that optional dependencies are part of "
          "Chromarchy's MIT license.");
}

QString aboutText() {
  return QStringLiteral(
             "About Chromarchy\n\n"
             "Chromarchy %1\n"
             "An original offline-first raster image editor for Omarchy "
             "Quattro and current Arch Linux/Wayland.\n\n"
             "Author: Rohan Patnaik\n"
             "License: MIT\n\n"
             "Chromarchy is not an Adobe product and contains no Adobe code, "
             "assets, branding, or generative-fill services. Capability claims "
             "are limited to the repository's tested offline parity catalog.")
      .arg(QApplication::applicationVersion().left(128));
}

}  // namespace

HelpDialog::HelpDialog(Page initialPage, QWidget* parent) : QDialog(parent) {
  setObjectName(QStringLiteral("offlineHelpDialog"));
  setWindowTitle(QStringLiteral("Chromarchy Offline Help"));
  setAccessibleName(QStringLiteral("Chromarchy offline help"));
  setAccessibleDescription(QStringLiteral(
      "Bundled local help, shortcuts, limits, diagnostics, licenses, and about"));
  setModal(true);
  resize(760, 560);

  auto* layout = new QVBoxLayout(this);
  tabs_ = new QTabWidget(this);
  tabs_->setObjectName(QStringLiteral("offlineHelpTabs"));
  tabs_->setAccessibleName(QStringLiteral("Offline help topics"));
  tabs_->setAccessibleDescription(
      QStringLiteral("Choose a bundled local help topic"));
  addPage(QStringLiteral("helpOverviewPage"), QStringLiteral("Overview"),
          QStringLiteral("Offline product overview"), overviewText());
  addPage(QStringLiteral("helpShortcutsPage"), QStringLiteral("Shortcuts"),
          QStringLiteral("Keyboard shortcut reference"), shortcutsText());
  addPage(QStringLiteral("helpFormatsLimitsPage"),
          QStringLiteral("Formats and Limits"),
          QStringLiteral("Supported formats and hard resource limits"),
          formatsAndLimitsText());
  addPage(QStringLiteral("helpDiagnosticsPage"),
          QStringLiteral("Diagnostics"),
          QStringLiteral("Bounded local runtime diagnostics"),
          diagnosticsText());
  addPage(QStringLiteral("helpLicensesPage"), QStringLiteral("Licenses"),
          QStringLiteral("Chromarchy and dependency license notice"),
          licenseText());
  addPage(QStringLiteral("helpAboutPage"), QStringLiteral("About"),
          QStringLiteral("Chromarchy product identity"), aboutText());
  tabs_->setCurrentIndex(qBound(0, static_cast<int>(initialPage),
                                tabs_->count() - 1));
  layout->addWidget(tabs_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  buttons->setObjectName(QStringLiteral("offlineHelpButtons"));
  buttons->setAccessibleName(QStringLiteral("Offline help actions"));
  buttons->setAccessibleDescription(
      QStringLiteral("Close the offline help dialog"));
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  buttons->button(QDialogButtonBox::Close)->setDefault(true);
  layout->addWidget(buttons);
  QWidget::setTabOrder(tabs_, buttons);
}

void HelpDialog::addPage(const QString& objectName, const QString& title,
                         const QString& accessibleDescription,
                         const QString& text) {
  auto* page = new QPlainTextEdit(this);
  page->setObjectName(objectName);
  page->setAccessibleName(title);
  page->setAccessibleDescription(accessibleDescription);
  page->setReadOnly(true);
  page->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  page->setPlainText(text.left(maximumPageTextCharacters));
  page->moveCursor(QTextCursor::Start);
  tabs_->addTab(page, title);
}

}  // namespace chromarchy
