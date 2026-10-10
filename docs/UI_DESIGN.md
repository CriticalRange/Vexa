# UI DESIGN

The VEXA design system lives in `app/src/main/java/com/critical/vexaemulator/ui/` and `ui/theme/`.

## Design Language

- "Terminal-tech": dark-first, dense, instrument-panel look.
- Near-black surfaces, hairline borders, no heavy elevation.
- Monospace readouts for logs, stats, and technical values.
- Cyan-to-violet accent gradient reserved for brand and primary CTAs.

## Theme Tokens

- `ui/theme/Color.kt`: surface stack `VexaBlack` / `SurfaceBase` / `SurfaceHi` / `SurfaceCard`; `Edge` hairlines; `Ink` hierarchy; semantic colors + soft variants; accent spectrum (`Cyan`/`Teal`/`Violet`/`Indigo`/`Pink`/`Blue`); `VexaAccent` color vs `VexaBrand` / `VexaBrandSubtle` brushes.
- `ui/theme/Type.kt`: `Typography` (sans) plus `MonospaceTypography` covering all slots.
- `ui/theme/Shape.kt`: `ShapeTokens` dp scale plus `SmallShape` / `CardShape` / `PanelShape`.
- `ui/theme/Theme.kt`: `VexaEmulatorTheme`, dark-first, transparent system bars.

## Component Map

- `Brand.kt`: `GradientText`, `BrandLogo`, `LogoSize`.
- `Labels.kt`: `SectionLabel`, `StatusDot`, `TinyPill`, `LevelBadge`.
- `LogColors.kt`: `levelBadge`, `toColor` / `toSoftColor`, category colors, `formatLogTimestamp`.
- `Panels.kt`: `PanelSurface`, `SectionCard`, `AccentEdge`.
- `Pickers.kt`: `PickerCard` + `onSelect`.
- `Metadata.kt`: `MetadataRow`, `KeyValueRow`.
- `Buttons.kt`: `VexaButton` (gradient CTA), `DangerButton`, `GhostButton`.
- `Switches.kt`: `SwitchRow`.
- `Logs.kt`: `LogRow`, `LogList`, `LogEmptyState`, `LogFilterChip`.
- `Effects.kt`: `vexaGlow`, `vexaShimmer`, `ScanlineOverlay`.
- `Loaders.kt`: `BootSpinner`, `PulsingDots`.
- `Stats.kt`: `StatPill`, `StatRow`.
- `Hud.kt`: `GameHudChip`.
- `Previews.kt`: `@Preview` gallery.

## Screens

- `MainActivity`: launcher flow picker -> detail -> run -> system -> logs.
- `GameActivity`: render surface + `GameHudChip` bar + `GameDebugPanel` overlay + `GameLoadingOverlay` boot screen.

## Rules

- No `material-icons` dependency; draw glyphs with `Canvas` or use text glyphs only.
- Dark-first: the UI is designed and verified on the dark scheme.
- Compile check via `assembleDebug`.
