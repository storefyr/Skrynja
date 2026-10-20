# 14 — Eye Candy Animations

**Status:** done (v1.9.19)
**Depends on:** 10 (inline streaming indicator)
**Touches:** `qml/components/TypingIndicator.qml` (new), `qml/components/MessageBubble.qml`, `qml/pages/ChatPage.qml`, `harbour-skrynja.pro`

Pure-QML animations, no extra dependencies, Harbour-compliant. All effects are gated on `mistralApi.isBusy` so nothing animates at rest (battery-friendly on Sailfish hardware).

## Effects

1. **TypingIndicator** — three highlight-colored dots bouncing in a staggered wave inside the pending assistant bubble (replaces the plain BusyIndicator). Classic chat idiom, ~500ms cycle with 140ms stagger.
2. **Scan line** — a thin light with a soft glow sweeping across the input separator while streaming (Knight Rider style, 1.1s loop, `Easing.InOutQuad`).
3. **Send button halo** — a circular ring expanding and fading around the pause button while a request is in flight (0.9s pulse).
4. **Message arrival transition** — new list items fade in with a slight `OutBack` overshoot scale (300ms). Loading a saved conversation cascades the effect.

## Design constraints

- Colors always derive from `Theme.highlightColor` — adapts to any ambience.
- No `QtQuick.Particles` for now: allowed by Harbour, but constant particle systems are costly on weaker SFOS devices; the current effects are cheap property animations.
- `Row` positioners own their children's x/y, so the typing dots animate an inner Rectangle inside fixed-size Item slots.

## Acceptance criteria

- [ ] Dots wave appears in the empty assistant bubble as soon as a message is sent, disappears at the first token.
- [ ] Scan line sweeps the separator during the whole request, stops when done or cancelled.
- [ ] Halo pulses around the pause icon while busy.
- [ ] New messages pop in smoothly; no animation runs while idle (check CPU/battery in Settings > Battery).
