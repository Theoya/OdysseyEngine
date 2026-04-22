# /clear-console

Clear the log panel buffer and optionally auto-clear when entering Play mode.

## Controls

- **Clear button**: Empties the ring buffer immediately
- **Clear on Play toggle**: When checked, the buffer auto-clears when `state_.mode` transitions from Edit → Play

## Edge Trigger Logic

```cpp
if (clear_on_play_ && state.mode == Mode::Play && last_mode_ != Mode::Play) {
    sink_->buffer().clear();
}
last_mode_ = state.mode;
```

## UI

Shown as a checkbox next to "Auto-scroll" in the control row.

## Notes

- State is NOT saved to editor preferences (reset each session)
- Useful for keeping logs tidy during gameplay
- Toggling off allows log accumulation across mode changes
