import QtQuick
import Quickshell

Item {
  id: root

  property string omarchyPath: ""
  property var shell
  property var manifest
  property var pluginRegistry

  function open(payloadJson) {
    Quickshell.execDetached([
      "sh",
      root.omarchyPath + "/scripts/launch-chromarchy"
    ])
  }

  function close() {}
}
