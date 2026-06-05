# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.5.1] - 2026-06-05

### Fixed
- `comfoair/commands/auto` had no effect when the unit was in `limited_manual`
  (the state produced by selecting a fan speed via touch or MQTT). The command
  only sent the operating-mode reset `85 15 08 01` (subunit `08`), but the
  firmware drives fan speed through `ventilation_level_X` = `84 15 01 01 …`
  (subunit `01`), so the active speed override was never cleared and the unit
  stayed in manual. `auto` now also sends the fan-speed reset `85 15 01 01`,
  returning the unit to auto from both `limited_manual` and `unlimited_manual`.
  Fixes [#13](https://github.com/vincentmakes/ComfoSense-Touch/issues/13).

## [1.5.0]

- Baseline release tracked from `main`.

[1.5.1]: https://github.com/vincentmakes/ComfoSense-Touch/compare/v1.5.0...v1.5.1
[1.5.0]: https://github.com/vincentmakes/ComfoSense-Touch/releases/tag/v1.5.0
