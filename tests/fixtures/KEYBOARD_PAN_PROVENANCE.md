# Keyboard-pan contract provenance

`keyboard-pan-contract.json` is an original Chromarchy viewport-navigation
contract. It fixes a 32-viewport-pixel arrow-key step and a four-times Shift
step independently of document pixels and zoom. The asymmetric document checks
both axes before and after quarter-turn view rotation.

The 300,000-square sparse case and odd 1,025 repetition count bound hostile
repeat behavior without scanning or allocating image tiles. The contract is
ephemeral per-tab display state; it does not define persisted view settings,
gestures, a navigator, or document-space movement.
