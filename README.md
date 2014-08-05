SECD machine
============

This is a loose implementation of [SECD machine](http://en.wikipedia.org/wiki/SECD) and a simple self-hosted Scheme-to-SECD compiler/interpreter.

Quick run:
```
$ ./secdscheme
;>> (+ 2 2)
   4

;>> (define n 10)
   n

;>> (define (sqr x) (* x x))
   sqr

;>> (define apply-to-42 (lambda (g) (g 42)))
   apply-to-42

;>> (apply-to-42 sqr)
1764

;>> (define (fact n) (if (eq? n 0) 1 (* n (fact (- n 1)))))
   fact

;>> (fact 10)
3628800

;>> (load "tests/lists.scm")
   ok

;>> (filter odd (range 12))
   (1 3 5 7 9 11)

;>> (begin (display 'bye) (quit))
bye
$
```

The design is mostly inspired by detailed description in _Functional programming: Application and Implementation_ by Peter Henderson and his LispKit, but is not limited by the specific details of traditional SECD implementations (like 64 Kb size of heap, etc) and R7RS.

Here is [a series of my blog posts about SECD machine](http://dmytrish.wordpress.com/2013/08/09/secd-about)

Opcodes and operational semantics
---------------------------------

The machine's state is represented as tuple of 4 lists:
* S for (computational) stack
* E for environment:
        Environment is a list of frames.
        Each frame is a pair (cons) of two lists:
            the first for symbol names,
            the second for bound values.
        The first frame represents the global environment and built-in routines.
* C for control path (list of opcodes to execute)
* D for dump (stack): it's for storing S/E/C that must be restored later

This state is written as `(s, e, c, d)`.

Notation: `(x.s)` means cons of value `x` and list `s`. Recursively, `(x.y.s)` means `(x.(y.s))`. An empty list may be written as `nil`, so `(v.nil)` is equal to `(v)`, `(x.y.nil)` to `(x y)`, etc.

**The current opcode set and the operational semantics**:

    ADD, SUB, MUL, DIV, REM
            :  (x.y.s, e, OP.c, d)     -> ((x OP y).s, e, c, d)
    LEQ     :  (x.y.s, e, LEQ.c, d)    -> ((x < y? #t : nil).s, e, c, d)

    CAR     :  ((x._).s, e, CAR.c, d)  -> (x.s, e, c, d)
    CDR     :  ((_.x).s, e, CDR.c, d)  -> (x.s, e, c, d)
    CONS    :  (x.y.s, e, CONS.c, d)   -> ((x.y).s, e, c, d)

    LDC v   :  (s, e, LDC.v.c, d)      -> (v.s, e, c, d)
    LD sym  :  (s, e, LD.sym.c, d)     -> ((lookup e sym).s, e, c, d)

    TYPE    :  (v.s, e, TYPE.c, d)     -> ((typeof v).s, e, c, d)
                where typeof returns a symbol describing variable type
    EQ      :  (v1.v2.s, e, EQ.c, d)   -> ((eq? v1 v2).s, e, c, d)

    SEL     :  (v.s, e, SEL.thenb.elseb.c, d)
                         -> (s, e, (if v then thenb else elseb), c.d)
    JOIN    :  (s, e, JOIN.nil, c.d) -> (s, e, c, d)

    LDF     :  (s, e, LDF.(args body).c, d) -> (clos.s, e, c, d)
                    where closure `clos` is ((args body).e);
                          `args` is a list of argument name symbols;
                          `body` is control path of the function.

    AP      :  (((args c') . e').argv.s, e, AP.c, d)
               -> (nil, (frame args argv).e', c', kont(s,e,c).d)
                    -- a closure ((args c1) . e') must be on the stack, 
                    -- followed by list of argument values `argv`.

               ((kont(s',e',c').d').(v).s, e, AP.c, d)
                -> (v.s', e', c', d')
                    -- run continuation with value v

    RTN     :   (v.nil, e', RTN.nil, kont(s,e,c).d) -> (v.s, e, c, d)

    APCC    : ((((arg) c') . e').s, e, APCC.c, d)
                -> (nil, (frame (arg=(kont(s, e, c) . d))).e', c', kont(s,e,c).d)
                    -- captures the continuation that can be called with AP

    DUM     :   (s, e, DUM.c, d)            -> (s, Ω.e, c, d)
    RAP     :   (clos.argv.s, Ω.e, RAP.c, d)
                -> (nil, set-car!(frame(args, argv), Ω.e'), c', s.e.c.d)
                    where `clos` is ((args c').(Ω.e'))

    PRINT   :  side-effect of printing the head of S:
                (v.s, e, PRINT.c, d) -> (v.s, e, c, d) -- printing v

    READ    :  puts the input s-expression on top of S:
                (s, e, READ.c, d) -> ((read).s, e, c, d)

There are functions implemented in C (`native.c`):
- `append`, `list`: heavily used by the compiler, native for efficiency;
- `eof-object?`, `secd-hash`, `defined?`;
- `secd-bind!` used for binding global variables like `(secd-bind! 'sym val)`. Top-level `define` macros desugar to `secd-bind!`;
- i/o related: `display`, `open-input-file`, `open-input-string`, `read-char`, `read-u8`, `read-string`, `port-close`;
- `secd`: takes a symbol as the first argument, outputs the following: current tick number with `(secd 'tick)`, prints current environment for `(secd 'env)`, shows how many cells are available with `(secd 'free)`; memory info with `(secd 'mem)`, the array heap layout with  `(secd 'heap)`.
- `interaction-environment` - this native form returns the current environment, the last frame is the global environment;
- vector-related: `make-vector`, `vector-length`, `vector-ref`, `vector-set!`, `vector->list`, `list->vector`;
- bytevectors: `make-bytevector`, `bytevector-length`, `bytevector-u8-ref`, `bytevector-u8-set!`, `utf8->string`, `string->utf8`;
- string-related: `string-length`, `string-ref`, `string->list`, `list->string`, `symbol->string`, `string->symbol`;
- `char->integer`, `integer->char`;

**About types:**
Supported types are (see `secd.h`, `enum cell_type`):
- CONSes that make persistent lists;
- ARRAYs, that implement Scheme vectors;
- STR, implementing UTF-8 encoded Unicode strings. Strings are immutable, contrary to R7RS;
- BYTES, implementing bytevectors as described in R7RS;
- SYMs, implementing symbols;
- INTs, a platform-specific `long int` C type; not an arbitrary-precision integer;
- CHARs, a unicode point below 0x10fff;
- FUNCs, built-in native functions;
- OPs, SECD operations;
- PORTs, Scheme I/O ports, file/string based, with other backends possible in the future;

Internal types:
- CELL_UNDEF, non-initialized value that is default for cells in non-initialized vectors;
- CELL_FRAME, a frame of the environment;
- CELL_ARRMETA: cells for arrays metainformation;
- CELL_REF: just a pointer to another cell; used to include NILs into arrays;
- CELL_FREE: this cell may be allocated;
- CELL_ERROR: contains an exception thrown by failed opcode execution;

Boolean values are Scheme symbols `#t` and `#f`. Any values except `#f` are evaluated to `#t`.

**Mutability and sharing**
Values are persistent, immutable and shared. Arrays are really handles for access to "mutable" memory.  Array cells are owned by its array and are copied on every access from Scheme. Setting a cell in array means destructing the previous value and initialization of the cell with copy of the new value. If you want to emulate mutable variables, use array boxing:
``` Scheme
;; emulating a counter object:
(define (make-counter)
  (let ((count (box 0)))     ;; desugars to (make-vector 1 0)
    (let ((get (lambda () (box-ref count)))  ;; desugars to (vector-ref count 0)
          ;; desugars to (vector-set! count 0 ...):
          (inc (lambda () (box-set! count (+ 1 (box-ref count))))))
      (lambda (msg)
        (cond
          ((eq? msg 'inc) (inc))
          ((eq? msg 'get) (get))
          (else 'do-not-understand))))))

(define counter (make-counter))
(counter 'get)  ;; => 0
(counter 'inc)
(counter 'get)  ;; => 1
```

**Memory management**:
Memory is managed using reference counting by default, a simple optional Mark&Sweep garbage collection is available via `(secd 'gc)` 

*Reference counting*. Every cell after allocation must be shared with `share_cell()` - this increments the refcount of the cell. When a cell is not used anymore, it must be `drop_cell()`d - it decrements the refcount and if it's 0 `free_cell()` is called. To initialize a cell of an array, use `copy_value()` of other cell. If a cell in an array must be set, previous value must be destructed with `drop_value()` to drop all its owned cells.

SECD occupes a contiguous region of memory divided into cells of type `cell_t` (starting at `secd->begin` and ending just before `secd->end`). The SECD heap is divided into 3 regions: _persistent heap_ from `secd->begin` to `secd->fixedptr`, _the free space_ from `secd->fixedptr` to `secd->arrayptr`, _array heap_ from `secd->arrayptr` to `secd->arrlist` (`secd->arrlist` is the last cell before `secd->end`). 
_Persistent heap_ is for quick (_O(1)_) allocation of one cell (of any type except CELL_ARRMETA and CELL_UNDEF). All cells in the persistent heap belong to one of five lists: `secd->stack`, `secd->env`, `secd->control`, `secd->dump`, `secd->free`. Some information about the persistent heap is available in REPL via `(secd 'mem)`. To view information about a specific cell by its number, use `(secd 'cell cell-num)` in REPL (like `(secd 'cell 1)`.
 
_Array heap_ is a sparse double-linked list of CELL_ARRMETA that reflects memory order: `meta->as.mcons.next` always points to the left adjacent CELL_ARRMETA and `meta->as.mcons.prev` to the right one; the list starts at `secd->arrlist` and grows to lesser addresses. Gaps between CELL_ARRMETA are arrays managed by the left adjacent CELL_ARRMETA. Arrays have their own refcount, so there may be any non-zero number of CELL_ARRAY/CELL_STRING/CELL_BYTES in the persistent heap or in cells of other arrays pointing to that CELL_ARRMETA space, so CELL_ARRAY are persistent as well and may be copied harmlessly using `copy_value()`. Arrays are: used/free, their space may be treated as bytes (for strings and bytevectors) or cells (for vectors) - this information is stored in CELL_ARRMETA. All cells in a vector have refcount 1 and must be copied into the persistent heap on every access. Information about heap layout is available in REPL via `(secd 'heap)`.

_Memory allocation/release_. Allocation of one cell is very quick: it uses head of `cell->free` double-linked list. If `secd->free` is empty, `secd->fixedptr` is incremented for persistent heap to grow into the free space. Allocation of arrays is more complicated: it's basically a first-fit allocator starting at `secd->arrlist` and looking for a free gap of sufficient size in the array heap, that gap is divided into the new array and possibly a smaller free gap. If there is no such gap, `secd->arrayptr` is moved into the free space to allocate an array of the given size just after `secd->arrayptr`.

If `secd->fixedptr` or `secd->arrayptr` can't be moved (there is no free space between them), SECD machine fails with `'error:_out_of_memory`.

Deallocation: if a cell from the persistent heap is released and it's adjacent to `secd->fixedptr`, `secd->fixedptr` is decremented to release all free cells adjacent to the free space. Otherwise the cell is prepended to `secd->free` list. If an array is released and its CELL_ARRMETA is at `secd->arrayptr`, `secd->arrayptr` is moved to reclaim its memory to the free space, otherwise this array is marked as free and all dependencies of its cells are `drop_cell`d; if there are free adjacent gaps, they are merged with the new one.

If you want to see the full machine state as a text file, do `(secd 'dump)` - the machine state will be serialized into file `secdstate.dump`.

**Garbage collection**
`(secd 'gc)` implements Mark&Sweep GC. See `secd_mark_and_sweep_gc()` in `memory.c` for details.

**Symbol storage**.
Symbol strings are stored in the _symstore_: it's a list of bytevector buffers in the array heap. Symbols are only created and can't be deleted (like in EVM, this allows to avoid reallocation of symbol on each repeated function call). Symbol string pointers are unique: if such symbol string already exists, a pointer to the existing string is shared, otherwise full (32-bit) hash of the string and this string with ending '\0' is appended into the last buffer in the symstore; if there is no space in that buffer, a new buffer is allocated. String lookup is implemented by a chained rebalancing hashtable that points to a symbol string by symbol hash.

**Input/output**: `READ`/`PRINT` are implemented as built-in opcodes in C code.  Scheme ports are half-implemented at the moment (see the list of native I/O functions).  `(load "path/to/file.scm")` is implemented by the interpreter.
To use a port for READ/PRINT, override variables `*stdin*` and `*stdout*` respectively:
```scheme
(let ((*stdin* (open-input-string "(+ 2 2)"))) (read)) ;=> '(+ 2 2)
```

**Tail-recursion**: added tail-recursive calls optimization.
The criterion for tail-recursion optimization: given a function A which calls a function B, which calles a function C, if B does not mess the stack after C call (that is, returns the value produced by C to A), we can drop saving B state (its S,E,C) on the dump when calling C. "Not messing the stack" means that there are no commands other than `JOIN`, `RTN` and combo `CONS CAR` (used by the Scheme compiler to implement `(begin)` forms) between `AP` in B and B's `RTN`. Also all `SEL` return points saved on the dump must be dropped.
The check for validity of TR optimization is done by function `new_dump_if_tailrec()` in `interp.c` for every AP.

Tail-recursion modifies AP operation to not save S,E,C of the current function on the dump, also dropping all conditional branches return points saved on the dump:

    AP (with TR)  :  ( ((args c').e').argv.s, e, AP.c, j1.j2...jN.d) 
                     -> (nil, frame(args, argv).e', c', d)
                        where j1, j2, ..., jN are jump return points saved by SELs in the current function.
    
    RTN           :  not changed, it just loads A's state from the dump in C's `RTN`.
                        

**Continuations**
SECDScheme supports compiling `call/cc` using a special opcode, APCC. Captured continuations may be called just like a function that takes a single argument:
```scheme 
;;;; Example #1
>>
 (define (any pred lst)
  (call/cc (lambda (return)
     (for-each
       (lambda (x) (if (pred x)
                       (return #t) ;; jump out, returning #t instead of call/cc body
                       #f)) lst)
     #f)))

;; Example #2
>> (define cc (box #f))
>> (define (val!) (call/cc (lambda (k) (box-set! cc k) 1)))
>> (+ 1 (* 10 (val!)))
   11
>> ((box-ref cc) 2)
   21
>> ((box-ref cc) 3)
   31
```

How to run
----------
`secdscheme` wrapper script tries to build silently all it needs:
``` bash
$ ./secdscheme
# compiles ./secd binary
# creates repl.secd from repl.scm if not found
>> (display "Hello Scheme!")
Hello Scheme!

```

Detailed process is:

First of all, compile the machine:
```bash
$ make
```

Examples of running SECD opcode sequences (lines starting with `>` are user input):

```bash
$ echo "(LDC 2  LDC 2  ADD PRINT)" | ./secd
4

$ ./secd < tests/test_fact.secd
720

# read from file first, then from the stdin
$ cat tests/test_io.secd - | ./secd
# or
$ ./secd tests/test_io.secd
> 1
(Looks like an atom)
> ^D
```

`secd` binary may be also used for interactive evaluation of control paths:
```bash
# without STOP, the control path is considered to be incomplete.
$ ./secd
> (LDC 2  LDC 2  ADD STOP)
4
```
See `tests/` directory for more examples of closures/recursion/IO in SECD codes.


Scheme compiler: scm2secd.scm and repl.scm
------------------------------------------

`scm2secd.scm` file a simple compiler from Scheme to SECD code. It is written in a quite limited subset of Scheme (using `let`/`letrec` instead of `define`, though now it supports `define` definitions). It does not have all the goodies of `repl.scm`, so keep in mind that `repl.scm` is compiled with `scm2secd.scm`, so the former must use (simpler) language compilable with `scm2secd.scm`.
`repl.scm` has much more featureful Scheme compiler.
There is a `define` macro. In top-level definitions it desugars to a native function `(secd-bind! 'symbol value)`. A macro can be defined with macro `define-macro` which works just like in Guile.

The compiler is self-hosted and can be bootstrapped using its pre-compiled SECD code in `scm2secd.secd`:

```bash
# self-bootstrapping:
$ ./secd scm2secd.secd <scm2secd.scm >scm2secd.1.secd
$ mv scm2secd.1.secd scm2secd.secd

# or, using a bootstrapping interpreter (tested with guile and mzscheme):
$ guile -s scm2secd.scm <scm2secd.scm >scm2secd.secd
$ mzscheme -f scm2secd.scm <scm2secd.scm >scm2secd.secd
```

Scheme expression and files may be evaluated this way:
```bash
$ cat tests/append.scm | ./secd scm2secd.secd | ./secd
```

Bootstrapping REPL: 
```bash
$ ./secd scm2secd.secd <repl.scm >repl.secd
$ ./secd repl.secd
>> (append '(1 2 3) '(4 5 6))
   (1 2 3 4 5 6)

>> (begin (display 'bye) (quit))
bye

$
```

Use `secd-compile` function to examine results of Scheme-to-SECD conversion in the REPL:
```scheme
>> (secd-compile '(+ 2 2))
   (LDC 2 LDC 2 ADD)

>> (let ((*stdin* (open-input-file "repl.scm"))) (secd-compile (read)))
   ... ;; full compiled code of repl.scm

>> (eval '(+ 2 2) (interaction-environment))
   4

```
