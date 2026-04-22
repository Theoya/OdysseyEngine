---
name: MPFB2 extension install path — Blender 4.2+ dual-install (RESOLVED 2026-04-21)
description: MPFB2 is incompatible with Blender 2.93 and MUST be installed as a Blender 4.2 *extension* (not a legacy add-on). Dual-install resolves this — Blender 2.93 for engine work, 4.2.20 portable for MPFB2. Install MPFB2 via bpy.ops.extensions.package_install_files, not the legacy addon_install path.
type: project
---

**RESOLVED 2026-04-21** — user approved dual-Blender install. MPFB2 v2.0.15 (build 20260421) now working in Blender 4.2.20 LTS portable at `C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\blender.exe`. See `project_blender_versions.md` for the dual-install convention.

MPFB2 (the MakeHuman Plugin For Blender 2) has a hard minimum of **Blender 4.2.0**. This is stated in the MPFB2 GitHub README, in the MakeHuman Community FAQ, and enforced by the add-on's `blender_manifest.toml` (`blender_version_min = "4.2.0"`). MPFB 2.0.15+ is packaged as a Blender **extension** (not a legacy add-on) and MUST be installed via `bpy.ops.extensions.package_install_files(filepath=..., repo='user_default', enable_on_install=True)`. The legacy `bpy.ops.preferences.addon_install(...)` path silently drops the zip into `scripts/addons/mpfb` but then fails at runtime with `ValueError: The "package" does not name an extension` because MPFB internally calls `bpy.utils.extension_path_user(...)` which requires registered-extension context. The enabled-module id becomes `bl_ext.user_default.mpfb`, not `mpfb`.

OdysseyEngine is pinned to **Blender 2.93 LTS** for existing engine-facing scripts (Berserk-Halo build, etc.). MPFB2 cannot run on 2.93.

The legacy predecessor `makehuman-plugin-for-blender` (MPFB1) requires Blender 2.8 AND a running standalone MakeHuman application via socket bridge — both deprecated and unmaintained. The MHX2 import/export path is similarly abandoned. Neither is a realistic substitute.

**Why:** the host project pins Blender 2.93 because the engine's existing asset pipeline (Berserk-Halo Mk3, the fps_humanoid demo, stick-figure skeleton rendering) was validated end-to-end on 2.93. Upgrading to 4.2+ is a user-level project-convention decision, not a silent agent action.

**How to apply:**
- For MPFB2-driven work, always invoke Blender 4.2.20 portable at `C:\Users\THadfield\Blender 4.2\blender-4.2.20-windows-x64\blender.exe`. Never 2.93.
- Install MPFB2 via `bpy.ops.extensions.package_install_files(filepath='<zip>', repo='user_default', enable_on_install=True)`. The zip source is `https://files2.makehumancommunity.org/plugins/mpfb2-<YYYYMMDD>.zip`.
- Enabled module id is `bl_ext.user_default.mpfb`. Operators live at `bpy.ops.mpfb.*` (135 operators as of 20260421 build).
- Extensions home for MPFB user data (skins, rigs, generated scripts): `%APPDATA%\Blender Foundation\Blender\4.2\extensions\.user\user_default\mpfb\`.
- If `bpy.ops.preferences.addon_install(...)` is ever used by mistake, MPFB will appear to install but fail at enable-time — clean `scripts/addons/mpfb` and retry with the extensions path.
</content>
</invoke>