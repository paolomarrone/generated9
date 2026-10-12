# Working on generated9

generated9 is a development branch of Plan 9/9front. Elegance, generality,
and simplicity of code and abstractions are primary design requirements.
A working implementation is necessary; a clear, coherent design is equally
necessary. Follow the idioms of the subsystem you are changing.

## Design

- Seek the smallest coherent mechanism that solves the problem. Remove
  unnecessary state, indirection, dependencies, and special cases.
- Generality comes from a precise interface and useful composition. Build
  abstractions around actual common structure, not hypothetical future uses.
- Prefer a few orthogonal operations to a growing collection of options,
  modes, callbacks, and exceptions. Keep policy separate from mechanism
  where doing so makes the design simpler.
- Make data representation, ownership, lifetimes, and invariants evident.
  Keep control flow direct and functions focused. Brevity must not obscure
  behavior or omit required error handling.
- Use existing Plan 9 mechanisms where they fit: files, 9P, namespaces,
  processes, pipes, and small composable programs. Understand their semantics
  before introducing a new interface or compatibility layer.
- Prefer fixing the underlying model to adding another workaround. Radical
  changes are welcome when they simplify the whole design; explain the
  resulting behavior and any compatibility consequences.

## Code style

- Read nearby code, its mkfiles, and relevant manuals before editing. Local
  conventions are the reference; do not impose a foreign style on the tree.
- In native user-space C, follow the existing Plan 9 conventions: `<u.h>`
  before `<libc.h>`, native types and interfaces, `nil`, `ARGBEGIN` where
  appropriate, and the established `exits`, `sysfatal`, and `%r` idioms.
  Kernel and library code must follow their own error and cleanup conventions.
- Match surrounding indentation, declarations, naming, and brace placement.
  Use short, meaningful names; add comments for intent, invariants, or subtle
  constraints rather than paraphrasing the code.
- Use `mk` and `rc` for native builds and scripts. Host bootstrap tools may
  use host languages when needed; keep that boundary small and explicit.
- Keep imported software's conventions within its own code. Isolate the
  necessary porting changes and avoid unrelated formatting churn.
- Validate real boundaries and handle failures explicitly. Avoid speculative
  checks and silent fallbacks that hide broken assumptions.

## Changes and validation

- Keep changes focused and reviewable. Avoid new dependencies, frameworks,
  configuration knobs, or helper layers without a concrete need.
- Test behavior at the level affected by the change. Prefer a small test
  that exposes a real regression over tests that mirror the implementation.
  Documentation-only edits need no runtime test.
- For the development VM, see `boot/README.md`. Use `boot/test` for changes
  to boot or shared filesystem behavior; use the native build and relevant
  subsystem tests for changes to 9front itself. Report what was actually tested.
- The VM writes directly into the checkout. Keep downloaded images, generated
  binaries, logs, and mutable guest state out of commits.

## History and attribution

- Work on `gen`. `origin` is `paolomarrone/generated9`; `upstream` is 9front.
  Preserve upstream history and keep commits small enough to understand.
- Preserve original authorship and license notices. Record the source and
  exact revision of imported or adapted code in `CREDITS.md` and, where
  useful, a local `UPSTREAM` file. Follow the component's own license.
- New generated9 contributions use the root `LICENSE`. Existing components
  retain their original licenses, including the exceptions in `lib/legal/NOTICE`.
