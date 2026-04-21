#pragma once

// ---------------------------------------------------------------------------
// scene_serializer.h
// Phase 2: write a SceneData back to .scene.xml preserving all attributes
// and child elements the loader did not explicitly know about.
//
// Design: a scene file may contain attributes or child elements that are
// not part of the base scene.xsd schema — e.g. light stubs, audio stubs,
// material_override on <entity>. The loader extracts KNOWN fields into
// SceneData::EntityDesc (for in-memory use) AND snapshots the raw
// unknown content per-entity / per-scene so the serializer can write
// them back without loss.
//
// Phase 2 round-trip contract: a load → serialize pass on an UNMUTATED
// SceneData produces byte-identical output. This is guaranteed by the
// serializer emitting the preserved source snapshot verbatim when
// SceneData::mutated is false.
//
// Phase 4+ will introduce a mutation path (Inspector edits → mutated=true
// → programmatic reconstruction with known-first ordering). That path is
// sketched in SerializeOptions::force_reconstruct for testability but
// not exercised in production code yet.
//
// Pure/impure split: `serialize_scene_to_string` is pure (in → out string).
// `serialize_scene` wraps it with file I/O.
// ---------------------------------------------------------------------------

#include "core/result.h"
#include "scene/scene_loader.h"

#include <filesystem>
#include <string>

namespace odyssey::scene {

struct SerializeOptions {
    // If true, ignore the preserved source snapshot and reconstruct the
    // XML programmatically from SceneData fields + unknown buckets.
    // Default false so an unmutated round-trip is byte-identical.
    bool force_reconstruct = false;

    // Indent string for reconstruction path. Ignored when echoing source.
    std::string indent = "  ";
};

// Pure function: emit the scene XML to a std::string. Returns the string
// on success, or an error message on malformed SceneData.
Result<std::string> serialize_scene_to_string(
    const SceneData& scene,
    const SerializeOptions& options = {});

// I/O boundary: write serialize_scene_to_string's output to `path`.
// Creates parent directories as needed. Writes atomically if the platform
// supports rename-on-existing (on Windows: write to tmp, then MoveFileEx).
Result<bool> serialize_scene(
    const SceneData& scene,
    const std::filesystem::path& path,
    const SerializeOptions& options = {});

} // namespace odyssey::scene
