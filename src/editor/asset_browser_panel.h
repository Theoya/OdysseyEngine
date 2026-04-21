#pragma once

// ---------------------------------------------------------------------------
// asset_browser_panel.h
// Phase 4 editor panel: walks the current project tree (defaulting to
// demo/showcase/) and presents every authoring asset grouped by type.
//
// Responsibilities:
//   - Classify each file on disk by its extension tail into an AssetType.
//   - Enumerate the tree (pure helper + thin filesystem boundary).
//   - Render a grouped tree view with colored placeholder icons — no
//     texture-based thumbnails yet; the bake pipeline is deferred to a
//     later phase (see skills/thumbnail-bake for the pinned contract).
//   - Selection updates EditorState::selected_asset so the Inspector's
//     preview pane can show the file contents (plain text for XML,
//     colored-keyword view for .nadir).
//   - Double-clicking a scene file swaps the editor's loaded scene,
//     refusing the swap when mode != Edit.
//   - Double-clicking a prefab is a no-op in Phase 4 (reserved for Phase 5
//     "spawn from prefab" path).
//
// Pure helpers (exposed for tests):
//   - classify_asset(path) -> AssetType
//   - enumerate_project(root) -> vector<AssetEntry>
//
// The tree is rebuilt on demand (menu click / root change) rather than
// watched — hot reload / ReadDirectoryChangesW is deferred to Phase 5+.
// ---------------------------------------------------------------------------

#include "editor/panel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace odyssey::editor {

// Classification of authoring assets the editor understands. Ordering of the
// enum is the ordering used by the tree view: meshes first (the visual
// primitives), then materials, then prefabs, then behaviors, lighting, music,
// scenes, actions, other. This ordering is contract-tested.
enum class AssetType {
    Mesh,           // *.mesh.xml
    Material,       // *.mat.xml
    Prefab,         // *.prefab.xml
    Behavior,       // *.nadir
    LightingProfile,// lighting_profiles/*.xml
    Music,          // *.music.xml, *.xml under music/
    Scene,          // *.scene.xml
    Actions,        // *.actions.xml
    Skeleton,       // *.skeleton.xml
    Animation,      // *.anim.xml
    Other,
};

// A single enumerated asset. Pure data — no pugi handles, no file content.
struct AssetEntry {
    std::filesystem::path path;    // absolute or whatever was passed in
    std::filesystem::path relative;// relative to the enumeration root
    AssetType             type = AssetType::Other;
    uintmax_t             size_bytes = 0;
};

// ---------------------------------------------------------------------------
// Pure helpers (tested directly in test_asset_browser.cpp)
// ---------------------------------------------------------------------------

// Classify a single asset path by its extension tail. Pure: never touches
// the filesystem. Double-extension matches (.mesh.xml, .mat.xml, etc.) are
// checked before the single-extension fallback (.xml -> LightingProfile if
// under lighting_profiles/, Music if under music/, Other otherwise — but
// context is encoded by the string match, see implementation).
AssetType classify_asset(const std::filesystem::path& p);

// Human-readable group label for an asset type (matches tree headers).
const char* asset_type_group_label(AssetType t);

// A stable sort key for an AssetType that reflects the UI grouping order
// (Meshes < Materials < Prefabs < Behaviors < LightingProfiles < Music <
// Scenes < Actions < Skeletons < Animations < Other). Smaller = earlier.
int asset_type_order_key(AssetType t);

// Recursively walk `root` and return every file classified as a known
// asset. I/O boundary: touches the filesystem but is deterministic given
// the same tree. Returns an empty vector (not an error) when `root` does
// not exist, so the editor can gracefully handle missing trees.
std::vector<AssetEntry> enumerate_project(const std::filesystem::path& root);

// Sort an enumeration in the canonical UI order: by asset_type_order_key,
// then by relative path lexicographically. Pure, in-place, exposed for
// tests to pin the ordering contract.
void sort_assets_canonical(std::vector<AssetEntry>& entries);

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

class AssetBrowserPanel : public Panel {
public:
    AssetBrowserPanel();

    const std::string& name() const override { return name_; }
    void draw(EditorState& state) override;

    // Trigger a re-enumeration against `state.project_root`. Cheap — the
    // showcase tree has well under 100 files. Called automatically on first
    // draw and whenever the user hits the Refresh button.
    void refresh(const std::filesystem::path& root);

private:
    std::string               name_ = "Asset Browser";
    std::filesystem::path     last_root_;
    std::vector<AssetEntry>   entries_;
    bool                      refreshed_once_ = false;
};

} // namespace odyssey::editor
