---
name: Always purge glTF_not_exported scaffolding before saving production blends
description: Blender's glTF importer auto-creates a "glTF_not_exported" collection for helper geometry (pivots, bounding spheres, transform anchors). If left visible, it shows up in the viewport on reopen even though it's tagged non-exportable. Clean it up before saving.
type: feedback
---
**When you import a .glb/.gltf and then save the resulting scene as a production .blend, audit the `glTF_not_exported` collection and either hide or delete it before saving.**

**Why:** Observed 2026-04-22 on the Obsidian Sentinel cloak bake. The Meshy-generated `Character_output.glb` imported with an `Icosphere` (2m dia, at origin, no materials) inside a `glTF_not_exported` collection. The collection's data-block `hide_viewport` flag was True, but the view-layer instance `hide_viewport` flag was False — so the sphere was visible in the saved viewport state. On reopen the user saw only the ball; the character and cape were hidden behind it. The renders themselves were clean (the render camera was positioned to avoid it or `hide_render` saved the day), but the authoring blend was broken for interactive use.

This tripped the user during hand-off and required a second Blender headless pass to clean up. Avoidable.

**How to apply:**
- After importing any .glb, check `bpy.data.collections.get('glTF_not_exported')`. If it exists, the importer added helpers.
- Decide per asset whether the scaffolding is useful:
  - **If useful** (e.g. visualization of import pivot): set both the data-block and view-layer `hide_viewport=True`. Also set `hide_render=True` on each member object as defense in depth.
  - **If not useful** (e.g. placeholder primitive, bounding sphere you don't need): delete the collection and its objects outright. Meshy imports almost always fall here.
- Before calling `wm.save_as_mainfile`, always verify the default 3D viewport's framing: set `view_location` near the character's center-of-mass and `view_distance` ~4-5× character height so the reopen view is sane.
- Add this check to every future glTF-based pipeline skill (Meshy imports, RPM, Mixamo, etc.).

**Quick snippet:**
```python
import bpy
c = bpy.data.collections.get('glTF_not_exported')
if c is not None:
    # Option A: delete outright
    for o in list(c.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.data.collections.remove(c)
    # Option B: hide instead (if you want to keep for debugging)
    # def hide_lc(lc, name):
    #     if lc.name == name: lc.hide_viewport = True
    #     for ch in lc.children: hide_lc(ch, name)
    # hide_lc(bpy.context.view_layer.layer_collection, 'glTF_not_exported')
```
