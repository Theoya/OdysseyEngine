---
skill: install_mpfb2
difficulty: beginner
prerequisites: [Blender 4.2+ installed — use the portable or MSI install at `C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\blender.exe` or `C:\Program Files\Blender Foundation\Blender 4.2\blender.exe`]
status: complete — verified end-to-end 2026-04-21, MPFB 2.0.15 build 20260421
blender target: 4.2 LTS (MPFB requires 4.2.0+)
---

## Goal

Install the MPFB2 add-on (MakeHuman Plugin For Blender 2) into Blender 4.2+ so headless scripts can drive MPFB2's Python API to generate CC0 humanoid base meshes.

## Critical — MPFB2 is a Blender 4.2 EXTENSION, not a legacy add-on

MPFB 2.0.x ships as a Blender 4.2 extension. Its zip contains a `blender_manifest.toml` (schema_version 1.0.0) instead of a legacy `bl_info` dict. Consequences:

- **Install via the extensions API**, not `bpy.ops.preferences.addon_install(...)`. The legacy path silently drops the zip into `scripts/addons/mpfb/` but the code path at `services/locationservice.py:29` calls `bpy.utils.extension_path_user(...)` which raises `ValueError: The "package" does not name an extension` and the enable fails.
- Post-install, the module id is `bl_ext.user_default.mpfb`, **not** `mpfb`. Operators are still exposed at `bpy.ops.mpfb.*` (135 operators as of build 20260421).
- User data home is `%APPDATA%\Blender Foundation\Blender\4.2\extensions\.user\user_default\mpfb\{data,config,cache,logs}\`, **not** `%APPDATA%\...\scripts\addons\mpfb\`.
- Blender 2.93 cannot run MPFB2 at all (manifest enforces `blender_version_min = "4.2.0"`).

## Sources (5 — complete)

1. [GitHub README — makehumancommunity/mpfb2](https://github.com/makehumancommunity/mpfb2) — 2026 — canonical install reference; states "MPFB 2.x requires a Blender version of at least 4.2"
2. [MakeHuman Community — MPFB Downloads](https://static.makehumancommunity.org/mpfb/downloads.html) — 2026 — nightly `mpfb2-YYYYMMDD.zip` mirror index at `https://files2.makehumancommunity.org/plugins/`.
3. [MakeHuman Community — How can I install MPFB2?](https://static.makehumancommunity.org/mpfb/faq/how_do_i_install.html) — 2026 — explicit extension-platform vs manual-zip paths for Blender 4.2.
4. [MakeHuman Community — Getting started](https://static.makehumancommunity.org/mpfb/docs/getting_started.html) — 2026 — post-install "new human → from scratch" flow.
5. [Blender Extensions platform — MPFB](https://extensions.blender.org/add-ons/mpfb/) — 2026 — in-Blender extension-platform install; enforces Blender 4.2+.

## Consensus ordered steps — headless install (verified 2026-04-21)

### Step 1 — Download the MPFB2 extension zip

```bash
curl -fSL -o "C:/Users/THadfield/Downloads/mpfb2-20260421.zip" \
  "https://files2.makehumancommunity.org/plugins/mpfb2-20260421.zip"
```

Current nightly zip is ~42-44 MB. Prefer the nightly over any stale "release" — per the authors, releases lag the Blender-API.

### Step 2 — Install as an extension via headless bpy

```python
import bpy
result = bpy.ops.extensions.package_install_files(
    filepath=r"C:\Users\THadfield\Downloads\mpfb2-20260421.zip",
    repo="user_default",
    enable_on_install=True,
)
assert result == {"FINISHED"}, f"MPFB install failed: {result}"
bpy.ops.wm.save_userpref()
```

The operator emits informational log lines ending with `mpfb.init: MPFB initialization has finished.` on success. It also auto-creates `%APPDATA%\Blender Foundation\Blender\4.2\extensions\.user\user_default\mpfb\{data,config,cache,logs}\`.

### Step 3 — Verify on a fresh Blender launch

```python
import bpy
have = any("mpfb" in a.module for a in bpy.context.preferences.addons)
assert have, "MPFB not present after restart"
assert hasattr(bpy.ops, "mpfb"), "bpy.ops.mpfb namespace missing"
print("MPFB2_READY — ops count:", len([n for n in dir(bpy.ops.mpfb) if not n.startswith("_")]))
```

Expect: `MPFB2_READY — ops count: 135`.

### Step 4 — (optional) Install asset packs

For skins, eyes, teeth, hair meshes, clothing presets — required if you want textured humans. For OdysseyEngine's current untextured BCn-baker-less pipeline you can skip this step and still generate rigged naked bases.

```python
import bpy
# NOTE: exact operator name verified via live session — adjust to match bpy.ops.mpfb.* surface
bpy.ops.mpfb.install_asset_pack_from_zip(filepath="/path/to/makehuman_system_assets.zip")
```

## Full end-to-end shell + bpy recipe (for reproducibility)

```bash
# 1. Download (one-time; ~42 MB)
curl -fSL -o "C:/Users/THadfield/Downloads/mpfb2-20260421.zip" \
  "https://files2.makehumancommunity.org/plugins/mpfb2-20260421.zip"

# 2. Install via headless Blender 4.2
"/c/Users/THadfield/Blender 4.2/blender-4.2.20-windows-x64/blender.exe" --background --python-expr "
import bpy
r = bpy.ops.extensions.package_install_files(
    filepath=r'C:\Users\THadfield\Downloads\mpfb2-20260421.zip',
    repo='user_default',
    enable_on_install=True,
)
print('install:', r)
bpy.ops.wm.save_userpref()
print('post-install addons:', [a.module for a in bpy.context.preferences.addons if 'mpfb' in a.module])
"

# 3. Verify persistence across Blender launches
"/c/Users/THadfield/Blender 4.2/blender-4.2.20-windows-x64/blender.exe" --background --python-expr "
import bpy
print('MPFB present:', any('mpfb' in a.module for a in bpy.context.preferences.addons))
print('bpy.ops.mpfb exists:', hasattr(bpy.ops, 'mpfb'))
print('op count:', len([n for n in dir(bpy.ops.mpfb) if not n.startswith('_')]))
"
```

## Gotchas

- **Legacy `addon_install` is wrong path.** It silently drops MPFB2 into `scripts/addons/mpfb/`, but enable fails with `ValueError: The "package" does not name an extension`. If you see this error, clean `%APPDATA%\Blender Foundation\Blender\4.2\scripts\addons\mpfb\` before retrying the extensions path. The extension install creates a completely different directory tree under `extensions\.user\user_default\mpfb\`.
- **Module id** is `bl_ext.user_default.mpfb`, not `mpfb`. Check `addon.module` with substring match.
- **Operator surface** remains at `bpy.ops.mpfb.*` even though module id is namespaced — the Blender extensions layer remaps the op namespace for backward compatibility.
- **Repo argument** — `repo='user_default'` is required; omitting it causes the op to prompt for a UI selection that never appears in headless mode.
- **Nightly vs release** — nightly `mpfb2-YYYYMMDD.zip` from files2 is preferred. The "release" tarball on GitHub can lag 2-3 months behind Blender API changes.
- **Blender 2.93 is NOT supported.** No workaround. Blender 3.x is likewise unsupported (extension platform is 4.2+).
- **Download throughput from files2.makehumancommunity.org is slow (~100 KB/s peak observed 2026-04-21).** Budget 5-10 minutes for the 42 MB download, not 30 seconds. Do NOT kill the curl process early.
- **Running MPFB2 on a machine with a stale `scripts/addons/mpfb/`** (e.g. from an abandoned legacy install attempt) will cause init to double-load or pick the broken copy. Clean stale paths first.
- **Asset packs are separate.** Without them, MPFB generates naked meshes with no textures — fine for OdysseyEngine's BCn-baker-less pipeline.

## Post-task update log

- 2026-04-21 — Session 9613f92d — Installed MPFB 2.0.15 build 20260421 into Blender 4.2.20 LTS portable at `C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\`. First attempt via `addon_install` failed as expected. `extensions.package_install_files(...)` succeeded cleanly. 135 MPFB operators confirmed. Skill status upgraded from `research-only` to `complete`.
