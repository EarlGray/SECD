Tasks Pending:
=============
| Task  | Description        
|-------|--------------------
| T1    | move symbols to the heap; 
| T2    | `ATOM_CHAR`: read/print, support, `char->int`
| T6    | move into indexes instead of `cell_t *` 
| F1    | FEATURE: fast symbol lookup
| F3    | FEATURE: non-blocking I/O
| F4    | FEATURE: green threads + mailboxes + messaging
| F5    | FEATURE: small FFI
| F6    | FEATURE: LLVM-backend

Tasks Done:
===========
| Task  | Description        
|-------|--------------------
| T3    | bytevectors, `utf8->string`
| T4    | reader: dot-lists
| T5    | refactor out `atom_type`
| T8    | `open-input-port`, `port?`, `read`, `read-u8`, `read-string`
| F2    | FEATURE: alternative garbade collection - (secd 'gc), mark & sweep
| T7    | polymorohic CAR/CDR; use arrays for `ATOM_OP`

Defects Pending:
===============
| Defect| Description
|-------|--------------------
| D3    | Crash on `(make-vector 1 '())`
| D5    | Crash on `(list->vector (read-file (open-input-file "repl.scm")))` (large lists?)

Defects Fixed:
=============
| Defect| Description
|-------|--------------------
|  D1   |  `(eq? "str" "str")` not handled
| D2    | Crash on reading `#()`
| D4    | Crash on freeing result of `(make-vector 2 'any)` -- part of T1
