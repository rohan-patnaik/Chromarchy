# Multi-tab quit lifecycle

Ctrl+Q resolves modified native documents in ascending tab order before the
window closes. Each dirty tab becomes current before its modal Unsaved Changes
prompt, so the document named by the prompt is also the visible and accessible
document context.

Save writes that document through the existing atomic native-save path and
continues. Discard leaves its source file unchanged and continues. Cancel stops
the sequence immediately, keeps every tab open, and leaves the canceled tab
active. A later quit attempt skips documents already saved clean and resumes
with the first remaining dirty tab. Clean tabs never prompt.

The checked-in three-document contract drives Ctrl+Q entirely by keyboard. Its
first attempt saves Alpha and cancels at Beta; its retry discards Beta and saves
Gamma. Native reopen checks distinguish both saved selections from the
unchanged discarded source. Prompt names, accessible metadata, default Save,
Save → Discard → Cancel focus order, active-tab context, and post-cancel dirty
state are verified offscreen. The fixture is fixed at three tiny documents and
does not establish an open-document quota or a shutdown-time guarantee.

Background-job cancellation, save-failure continuation policy, crash or
power-loss behavior, pointer-driven tab closing, and live Wayland or assistive-
technology behavior remain incomplete.
