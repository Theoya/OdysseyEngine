#include "editor/component_clipboard.h"

namespace odyssey::editor {

ComponentClipboard& component_clipboard() {
    static ComponentClipboard clipboard;
    return clipboard;
}

} // namespace odyssey::editor
