# Multi-tab quit contract provenance

`multi-tab-quit-contract.json` is an original Chromarchy document-lifecycle
contract. Three independently saved native documents are dirtied with the
existing Select All command, then resolved in ascending tab order.

The first quit attempt saves Alpha and cancels at Beta. All tabs remain open,
the canceled tab stays active, Alpha is clean and persisted, and later tabs are
untouched. The retry skips clean Alpha, discards Beta, saves Gamma, and closes.
Reopening the files distinguishes persisted Save from source-preserving
Discard. This bounded fixture does not claim background-job cancellation,
save-failure recovery, or an application-wide open-document quota.
