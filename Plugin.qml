import QtQuick
import Quickshell
import Quickshell.Io

Item {
  id: root

  property string omarchyPath: ""
  property var shell
  property var manifest
  property var pluginRegistry

  function open(payloadJson) {
    if (!launcher.running)
      launcher.running = true
  }

  function close() {}

  Process {
    id: launcher
    command: [
      "sh",
      "-c",
      "if command -v chromarchy >/dev/null 2>&1; then exec chromarchy; fi; "
        + "message='Install the native chromarchy binary on PATH, then try again.'; "
        + "printf 'Chromarchy: %s\\n' \"$message\" >&2; "
        + "if command -v notify-send >/dev/null 2>&1; then "
        + "notify-send --app-name=Chromarchy 'Chromarchy is not installed' \"$message\"; fi; "
        + "exit 127"
    ]
  }
}

