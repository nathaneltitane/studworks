# Code instructions

- Use PascalCase for local variables, prefix class member variables with m and globals with g.
- Keep the file lists in `studworks.pro` and the module `.pri` files alphabetically sorted within their existing groups.
- Name source files category first, then subject and qualifier: `sw_dialog_render`, `sw_widget_label_elided`, `sw_object_camera`.
- Split a large class by feature into `sw_<class>_<area>.cpp` files that share its header, like `sw_model_selection.cpp` and `sw_window_main_command.cpp`.
- Get Qt and std includes through the precompiled header, `sw_global.h`.
- Prefer std containers, use Qt containers where a Qt API boundary makes them useful.
- Prefer `swstrcpy()` and `swstrcat()` from `sw_string.h` for bounded C-string copies and concatenation. Use the array overloads when possible; avoid `strncpy()` plus manual null termination. Keep standard functions where no suitable `sw` helper exists.
- Avoid using `auto` for ordinary variable types. Template iterators, lambdas, and structured bindings may use it for readability.
- Do not nest ternary expressions. Use explicit branches when a choice needs another condition.
- Prefer a private static member function over a file-scope helper when the function belongs to a class and does not use instance state.
- Prefer explicit `lock()` and `unlock()` over an extra scope with `QMutexLocker` for short, straight-line critical sections that must end before the following code. Keep `QMutexLocker` for early returns, waits, or nontrivial control flow.
- Preserve existing blank lines when editing code. Prefer blank lines between declarations and following logic, and between distinct logical steps; do not remove user-added blank lines without a specific reason.
- When adding a parameter to an existing function, update every call site explicitly instead of adding a default argument to preserve old calls.
- List new file meta commands in `references/command-meta.md`, and add their object to `gCommandMetaObjects`.

# Commit Messages

- Every nontrivial commit must have a concise subject and a descriptive body.
- Explain the problem or trigger, why the change is needed, and how the resulting behavior addresses it.
- Write for a future maintainer without access to the chat. Do not merely restate the diff or include conversational history.
- Subject-only messages are acceptable for purely mechanical changes.

# Dialog help and translations

- Follow `documentation/dialog-help.md` when adding or changing dialog controls, including help text, label buddies, and Help/F1 behavior.
- Keep UI strings translatable.
