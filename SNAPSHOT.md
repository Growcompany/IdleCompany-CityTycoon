# Snapshot

- Private source revision: `4da187336659282d2eb5b5dd7f90c86e7cf0ec52`
- Snapshot date: `2026-10-23`
- Unreal Engine: `5.4`
- Public source files under `Source/`: `1510`
- Public C++ test source files under `Private/Tests/`: `242`
- Public C++ automation test declarations: `772`
- Public `GameInstanceSubsystem` domain services: `38`
- Working-tree changes excluded from export: `yes`

The private Azure repository remains the source of truth. This GitHub repository is a one-way, sanitized source snapshot. The committed private revision above was used for the export; uncommitted private changes were excluded. Four committed PNG diagnostics under `Source/` were excluded because this is a source-only repository. The Firebase fallback URL in `ChatManagerSubsystem.cpp` and personal paths in `Tools/CityOpt` were replaced with portable examples. `Tools/RemoteExec/remote_cmd.py` reads the engine installation from `UE_ROOT`.

## Runnable tool checks

Checks below were run against this public snapshot on `2026-10-23`.

| Check | Result | Command |
|---|---|---|
| Node balance simulator | 128 passed · 1 failed · 6 conditional skips (135 total) | `node --test "Tools/Balance/test/*.test.js"` |
| Python rendering contracts | 110 passed · 0 failed | `py -3 -m unittest discover -s Tools/MainMapPreview -p "test_*.py"` |
| Cheat command documentation | 122 C++ `Exec` commands match the Markdown and HTML docs | `py -3 Tools/CheatDoc/verify_cheat_docs.py` |

The balance failure is the repository coverage check: `Tools/Balance/verify/coverage.json` from the same private revision has no entries for 27 resource/item API call sites in the new C++ source. The simulator's other 128 cases passed. The previous snapshot's 239 passing checks applied only to revision `566fa3af6170cb59e15ee4ffeaf6a861cef4dcde` and are not a result for this revision.

C++ automation tests are counted as sources and declarations only. Running them requires the excluded maps, assets and plugins, so this snapshot makes no full-pass claim for them. Build success, runtime success, Editor success and Android on-device success are not used as evidence for one another.
