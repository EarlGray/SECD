Tasks Pending:
=============
| Task  | Description        
|-------|------------------------------------------------
| T6    | move to indexes instead of `cell_t *`  - ?
| T7    | audit for refcounting correctness
| T8    | Scheme parser in Scheme
| T9    | Scheme language testing framework + tests
| T11   | static check for TCO during function compilation
| T12   | fast environment lookup: self-bal. tree? Static analysis for free variables? -> LDV index
| T13   | inline `let` lambdas: ST opcode
| T14   | static analysis: rewrite simple tail-call iterations into loops
| T16   | change #.OP syntax to be compatible with other interpreters; eliminate compile_control_path() ? Scheme enum type?
| T19   | `letrec*`, `let*`; analyses for dependencies in large `letrec*`s
| T20   | numbers: floating point, libgmp?
| T21   | `eval` does not use the supplied environment
| F3    | FEATURE: non-blocking I/O
| F4    | FEATURE: green threads + mailboxes + messaging
| F5    | FEATURE: small FFI, native modules as .so
| F6    | FEATURE: LLVM-backend

Tasks Done:
===========
| Task  | Description        
|-------|--------------------
| T1    | move symbols to the heap; 
| T2    | `ATOM_CHAR`: read/print, support, `char->int`
| T3    | bytevectors, `utf8->string`
| T4    | reader: dot-lists
| T5    | refactor out `atom_type`
| T7    | polymorhic CAR/CDR; use arrays for `ATOM_OP`
| T8    | `open-input-port`, `port?`, `read`, `read-u8`, `read-string`
| T10   | static check for stack correctness: tests/secdtool.scm:valid-stack
| T15   | exception handling; dynamic environment?
| T17   | arity checks for function calls
| T18   | or/and macros
| F1    | FEATURE: fast symbol lookup
| F2    | FEATURE: alternative garbade collection - (secd 'gc), mark & sweep

Defects Pending:
===============
| Defect| Description
|-------|--------------------
| D5    | Crash on `(list->vector (read-file (open-input-file "repl.scm")))` (large lists?)
| D6    | the Yin-Yang call/cc puzzle does not work as expected

Defects Fixed:
=============
| Defect| Description
|-------|--------------------
| D3    | Crash on `(make-vector 1 '())`
| D1    |  `(eq? "str" "str")` not handled
| D2    | Crash on reading `#()`
| D4    | Crash on freeing result of `(make-vector 2 'any)` -- part of T1
| D7    | secdtool:free-variables does not handle #.DUM - #.RAP correctly
| D8    | crash on (secd 'gc)
