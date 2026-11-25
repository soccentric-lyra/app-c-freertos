# Planned improvements: app-c-freertos

Gaps found in review and the enhancements planned to close them, highest priority first. Priority 1 is next, 3 is later.

## Gaps

| Area | Gap |
|---|---|
| Tests | No unit tests, although `crc32`, `hwlink` framing and the LED and button logic are portable. |
| CI | No GitHub Actions workflow, so there is no visible build status. |
| Docs | Doxygen exists, but nothing says how the pieces fit together (task model, memory map, boot hand-over). Generated output is not published. |
| README | Its "Related projects" list describes `app-rust-freertos` as "Rust on Embassy". It should say Rust on FreeRTOS. |
| Quality gates | No coverage, no sanitizer build, no `cppcheck` or MISRA-style check, although the code is aimed at products. |
| Docs hygiene | `CHANGELOG.md` has only `[Unreleased]` while the history reads as monthly releases. |

## Enhancements

| Priority | Enhancement | Result |
|---|---|---|
| 1 | Host-side unit tests (Unity or Ceedling) for `crc32`, COBS framing, `hwlink` parsing, pattern and debounce logic, built natively in Docker | First real tests, plus a base the other C repo can reuse |
| 1 | GitHub Actions workflow: build, `make lint-strict`, footprint, tests, with a README badge | Visible green status |
| 2 | `docs/architecture.md`: task diagram, memory map (slot A at `0x0800C000`), boot hand-over record, HWLINK sequence | Shows the design, not just the code |
| 2 | Publish Doxygen output to GitHub Pages | Browsable API docs |
| 2 | Add `cppcheck` and a coverage report (gcov) for the host tests | Quality evidence |
| 3 | Run the image in Renode or QEMU and check the boot log lines | Hardware-free validation |
| 3 | Move the HWLINK framing into a shared, tested module used by both C repos | Removes the 454 vs 506 line copies that can drift |
| 3 | Release notes and a tag per release | Maintained look |

