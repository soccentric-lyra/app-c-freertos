# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Host unit tests for CRC-32/ISO-HDLC and the COBS/HWLINK framing layer,
  runnable with `make test-host`, with a gcov coverage mode and an
  AddressSanitizer/UndefinedBehaviorSanitizer run
- `cppcheck` static analysis target and a GitHub Actions workflow for host
  tests, firmware build, strict lint and footprint
- Doxygen output published to GitHub Pages by a second workflow
- `docs/architecture.md`: task model, flash/RAM map, boot hand-over and
  HWLINK sequence
- `scripts/check_boot_contract.py`, shared by Jenkins and GitHub Actions

### Changed

- The Jenkins boot-contract stage now calls the shared script instead of
  embedding a second copy of the expected layout

### Fixed

- The README described `app-rust-freertos` as "Rust on Embassy"; it is Rust
  on FreeRTOS

## [1.0.0] - 2026-11-19

### Added

- FreeRTOS demo application in C for the NUCLEO-F767ZI: selectable LED
  patterns, user button handling and HWLINK communication over UART
- Watchdog supervision and self-confirmation after a health check so the
  bootloader keeps the new image
- Runs from either A/B slot under the bootloader
- Reference implementation the Rust and Zephyr demos are measured against

[Unreleased]: https://github.com/soccentric-stm32/app-c-freertos/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/soccentric-stm32/app-c-freertos/releases/tag/v1.0.0
