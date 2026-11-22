# Deep Harbor — Patch 2.4

*Released 22 November 2026 · Build `2.4.0-rc3`*

> [!IMPORTANT] Save Compatibility
> Existing harbor saves migrate automatically on first load. Rollbacks to `2.3.x` are not supported once you dock at **Bellwatch Pier**.

The **grapple hook** no longer clips through thin seawalls, lantern physics stay steady at high frame rates, and the second boss phase now gives a clearer windup cue.

> [!TIP] Harbor Shortcut
> Hold `Shift` while reeling in the line to vault directly onto catwalk ledges without spending stamina.

## Balance Changes

| Encounter / Tool | Category | Previous | New | Delta |
| :--------------- | :------: | -------: | --: | ----: |
| Sunken Leviathan (Phase 2) | Boss HP | `4,200` | `3,570` | `-15%` |
| Abyssal Slam Telegraph | Timing | `0.40s` | `0.70s` | `+75%` |
| Brass Grapple Reel Speed | Mobility | `18 m/s` | `21 m/s` | `+17%` |
| Saltwater Tonic Cooldown | Consumable | `12.0s` | `9.5s` | `-21%` |

## Release Checklist

- [x] Fix grapple hook clipping through thin dock bulkheads
- [x] Prevent cutscene autosaves from clearing quick-slot inventory
- [x] Duck ambient surf audio cleanly beneath NPC dialogue lines
- [ ] Calibrate inverted rumble curves on third-party gamepads (`2.4.1`)

> [!NOTE]
> Full telemetry notes live in the [Harbor Changelog](https://example.com/harbor) or via <support@example.com>.
