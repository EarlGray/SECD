Tasks Pending:
=============
| Task  | Description        
|-------|--------------------
| T1    | move symbols to the heap; 
| T2    | `ATOM_CHAR`: read/print, support, `char->int`
| T3    | bytevectors, `utf8->string`
| T4    | reader: dot-lists
| T5    | refactor out `atom_type`
| T6    | move into indexes instead of `cell_t *` 
| T7    | polymorohic CAR/CDR; use arrays for `ATOM_OP`
| T8    | `open-input-port`, `port?`, `read`, `read-u8`, `read-string`
| F1    | FEATURE: fast symbol lookup
| F2    | FEATURE: alternative garbade collection
| F3    | FEATURE: non-blocking I/O
| F4    | FEATURE: small FFI
| F5    | FEATURE: green threads + mailboxes + messaging
| F6    | FEATURE: LLVM-backend

Tasks Done:
===========
| Task  | Description        
|-------|--------------------


Defects Pending:
===============
| Defect| Description
|-------|--------------------
| D2    | Crash on reading `#()`
| D3    | Crash on `(make-vector 1 '())`
| D4    | Crash on freeing result of `(make-vector 2 'any)` -- part of T1

Defects Fixed:
=============
| Defect| Description
|-------|--------------------
|  D1   |  `(eq? "str" "str")` not handled
