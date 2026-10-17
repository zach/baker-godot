# Working on this integration

This repository integrates Baker with Godot. Baker is MIT-licensed and must stay free of
Godot's terms; baker-dev's `INTEGRATIONS.md` is the policy. For work here:

- **Nothing from this repository goes into Baker.** Do not copy code, identifiers, comments,
  structure or documentation from here, or from Godot, into baker-dev or zach/baker, and do
  not write Baker code from memory of Godot's.
- **A need goes to Baker as a requirement note,** in `requirements/`, from
  `requirements/0000-template.md`, written in Baker's terms: what a host needs, why as a
  general use case, and the constraints. The owner takes the note's text to Baker.
- **Confidential material stays here.** What Godot's SDK or agreement discloses does not go
  into a requirement note, a Baker issue, a pull request, or a Baker session.
- **Do not start Baker work from this session.** Changes to Baker happen in a separate
  session in baker-dev, from the note alone.
- **Use Baker as a user would:** a released version (zach/baker), through its public
  headers. A workaround for a Baker limit lives here until Baker ships the feature; a
  feature on baker-dev main reaches this repository at Baker's next release.

## This repository

- Godot 4.5 and godot-cpp `godot-4.5-stable`: one version line; a new minor version is a
  deliberate update.
- No Godot engine source. The sample is written here from scratch.
- Before each commit: `clang-format-18 -i src/*`, build `template_debug` and
  `template_release`, then run `sample/tests/runtime_test.tscn`, the `jump` scenario, and
  `tests/e2e.sh` headless (README). A fresh headless import can crash on exit; import twice.
- Scope is tunables and rules, never scenes.
- The owner has asked that each new requirement note's full text be sent to the baker-dev
  session working on Baker (`session_016xH75dZw1uN5knrqdhWRzT`) once it is written, and
  nothing from this repository beyond the note.
