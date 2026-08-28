# Background tab-close pointer workflow

Each document tab exposes a named close button. Activating a non-current tab's
button first makes that document current so any Unsaved Changes prompt has the
correct visual and accessible context.

Cancel keeps all tabs open with the targeted document current and returns focus
to its canvas. Retrying and choosing Discard closes only that target, leaves its
source file byte-for-byte unchanged, selects the deterministic tab now occupying
the removed index (or the preceding tab when no successor exists), and focuses
the remaining canvas. Clean tabs use the same target/current/focus transition
without a prompt.

The permanent offscreen contract uses three empty 300,000 × 300,000 sparse
native documents. It dirties only the target's sparse selection metadata,
clicks the actual tab close button, exercises pointer Cancel and Discard,
verifies accessible button/prompt metadata and target context, and proves zero
pixel tiles plus exact source preservation.

This remains a strict Partial lifecycle workflow. Save failure, background-job
cancellation, large tab populations, live Wayland/assistive-technology behavior,
middle-click conventions, and a measured end-to-end user-decision deadline
remain incomplete.
