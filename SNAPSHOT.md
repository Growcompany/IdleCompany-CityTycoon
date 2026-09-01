# Snapshot

- Private source revision: `566fa3af6170cb59e15ee4ffeaf6a861cef4dcde`
- Snapshot date: `2026-09-01`
- Unreal Engine: `5.4`
- Public source files under `Source/`: `859`
- Public C++ automation test source files: `62`
- Public C++ automation test declarations: `148`
- Public `GameInstanceSubsystem` domain services: `29`
- Working-tree changes excluded from export: `yes`

The private Azure repository remains the source of truth. This GitHub repository is a one-way, sanitized source snapshot.

## Runnable tool checks

Counts verified on the revision above.

| Check | Result | Command |
|---|---|---|
| Node balance simulator | 129 passed · 6 conditional skips · 0 failed (135 total) | `node --test "Tools/Balance/test/*.test.js"` |
| Python rendering contracts | 110 passed · 0 failed | `py -3 -m unittest discover -s Tools/MainMapPreview -p "test_*.py"` |
| Cheat command documentation | 85 C++ `Exec` commands match the Markdown and HTML docs | `py -3 Tools/CheatDoc/verify_cheat_docs.py` |

Total: `239` passed. The Python rendering checks require `NumPy` and `Pillow`.

C++ automation tests are counted as sources and declarations only. Running them requires the excluded maps, assets and plugins, so this snapshot makes no full-pass claim for them. Build success, runtime success, Editor success and Android on-device success are not used as evidence for one another.
