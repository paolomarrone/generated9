# Working on generated9

generated9 is a development branch of Plan 9/9front. Elegance, generality
and simplicity of code and abstractions are primary requirements.

## Design and style

- Use the smallest coherent mechanism. Remove unnecessary state, indirection,
  dependencies and special cases. Fix the model rather than add workarounds.
- Build abstractions around shared structure, not hypothetical uses.
  Prefer orthogonal operations; keep policy separate from mechanism.
- Make ownership, lifetimes and invariants clear. Brevity must preserve
  correctness and error handling; avoid speculative checks and silent fallbacks.
- Use Plan 9 files, 9P, namespaces, processes and pipes where they fit.
  Understand their semantics before adding interfaces or compatibility layers.
- Preserve upstream interfaces, protocols, formats and build conventions when
  practical. Explain necessary incompatibilities and how users migrate.
- Read nearby code, mkfiles and manuals. Match local style, names and types.
  Native user C uses <u.h> before <libc.h>, nil, ARGBEGIN and Plan 9 errors;
  kernel and library code follow their subsystem's conventions.
- Comment intent and non-obvious constraints. Keep control flow direct.
- Use mk and rc for native work. Keep host bootstrap dependencies separate.
  Preserve imported code's conventions and isolate porting changes.

## Changes and validation

- Keep changes focused. Add dependencies, frameworks or options only for a
  concrete need. Radical simplification is welcome; explain its consequences.
- Test behavior and real failure boundaries, not implementation details.
  Run relevant native checks and report what ran. Documentation needs no
  runtime test. Use boot/test for boot or file-sharing changes.
- See boot/README.md for the VM. It writes directly into this checkout;
  do not commit downloads, binaries, logs or mutable guest state.
- Keep documentation terse. Put interfaces in manuals, build/test instructions
  in READMEs and provenance in UPSTREAM/CREDITS.md. Link rather than repeat.
  Preserve commands, limits and license notices; omit session narratives.

## History and attribution

- Work on gen. origin is paolomarrone/generated9; upstream is 9front.
  Preserve upstream history and keep commits small and reviewable.
- Preserve authorship and license notices. Record imported/adapted sources
  and exact revisions in CREDITS.md and, where useful, a local UPSTREAM file.
- New contributions use the root LICENSE. Existing components keep their
  licenses, including the exceptions in lib/legal/NOTICE.
