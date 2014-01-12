SECD machine
============

This is a loose implementation of [SECD machine](http://en.wikipedia.org/wiki/SECD) and a simplest self-hosted Scheme-to-SECD compiler.

The design is mostly inspired by detailed description in _Functional programming: Application and Implementation_ by Peter Henderson and his LispKit, but is not limited by the specific details of traditional SECD implementations (like 64 Kb size of heap, etc).

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

    ATOM    :  (v.s, e, ATOM.c, d)     -> ((atom? v).s, e, c, d)
    EQ      :  (v1.v2.s, e, EQ.c, d)   -> ((eq? v1 v2).s, e, c, d)

    SEL     :  (v.s, e, SEL.thenb.elseb.c, d)
                         -> (s, e, (if v then thenb else elseb), c.d)
    JOIN    :  (s, e, JOIN.nil, c.d) -> (s, e, c, d)

    LDF     :  (s, e, LDF.(args body).c, d) -> (clos.s, e, c, d)
                    where closure `clos` is ((args body).e);
                          `args` is a list of argument name symbols;
                          `body` is control path of the function.

    AP      :  (((args c') . e').argv.s, e, AP.c, d)
               -> (nil, (frame args argv).e', c', s.e.c.d)
                    -- a closure ((args c1) . e') must be on the stack, 
                    -- followed by list of argument values `argv`.

    RTN     :   (v.nil, e', RTN.nil, s.e.c.d) -> (v.s, e, c, d)

    DUM     :   (s, e, DUM.c, d)            -> (s, Ω.e, c, d)
    RAP     :   (clos.argv.s, Ω.e, RAP.c, d)
                -> (nil, set-car!(frame(args, argv), Ω.e'), c', s.e.c.d)
                    where `clos` is ((args c').(Ω.e'))

    PRINT   :  side-effect of printing the head of S:
                (v.s, e, PRINT.c, d) -> (v.s, e, c, d) -- printing v

    READ    :  puts the input s-expression on top of S:
                (s, e, READ.c, d) -> ((read).s, e, c, d)

There are some functions implemented in C for efficiency:
- `append`, `list`, `null?`, `copy`: are heavily used by the compiler, native for efficiency;
- `number?`, `symbol?`, `eof-object?`: may be implemented in native code only;
- `secdctl`: takes a symbol as the first arguments, outputs the following: stack usage for `(secdctl 'stack)`, prints current environment for `(secdctl 'env)`, shows how many cells are available with `(secdctl 'free)`;
- `interaction-environment` - this native form gets the current environment (there's no distinction between lexical and dynamical environment as in other Scheme implementations).

**About types:**
Supported types are CONSes, INTs and SYMs (and native functions, FUNCs, under the hood).
Boolean values are symbols `#t` and `nil` for now. Any values except `'()` and `nil` are evaluated to `#t`.
Only C `int` types are supported as integers

Values are persistent, immutable and shared.

**Memory management**:
Memory is managed using reference counting at the moment, a simple optional garbage collection is on my TODO-list. This means no contiguous memory allocation, thus no Scheme's strings, bytevectors, vectors, etc, only values composed from CONS'es, INTs, SYMs.

**Input/output**: `READ`/`PRINT` are implemented as built-in commands in C code.

**Tail-recursion**: added tail-recursive calls optimization.
The criterion for tail-recursion optimization: given a function A which calls a function B, which calles a function C, if B does not mess the stack after C call (that is, returns the value produced by C to A), we can drop saving B state (its S,E,C) on the dump when calling C. "Not messing the stack" means that there are no commands other than `JOIN`, `RTN` and combo `CONS CAR` (used by the Scheme compiler to implement `(begin)` forms) between `AP` in B and B's `RTN`. Also all `SEL` return points saved on the dump must be dropped.
The check for validity of TR optimization is done by function `new_dump_if_tailrec()` in `secd.c` for every AP.

Tail-recursion modifies AP operation to not save S,E,C of the current function on the dump, also dropping all conditional branches return points saved on the dump:

    AP (with TR)  :  ( ((args c').e').argv.s, e, AP.c, j1.j2...jN.d) 
                     -> (nil, frame(args, argv).e', c', d)
                        where j1, j2, ..., jN are jump return points saved by SELs in the current function.
    
    RTN           :  not changed, it just loads A's state from the dump in C's `RTN`.
                        

How to run
----------
First of all, compile the machine:
```bash
$ gcc secd.c -o secd
```

Examples of running the SECD codes (lines starting with `>` are user input):

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


Scheme compiler: scm2secd.scm
-----------------------------

This file a simplest compiler from Scheme to SECD code. It is written in a quite limited subset of Scheme (even without `define`, using `let`/`letrec` instead). It supports very limited set of types (`symbol`s, `number`s and `list`s: no vectors, bytestrings, chars, strings, etc).

The compiler is self-hosted and can be bootstrapped using its pre-compiled SECD code in `scm2secd.secd`:

```bash
# self-bootstrapping:
$ ./secd scm2secd.secd <scm2secd.scm >scm2secd.1.secd
$ mv scm2secd.1.secd scm2secd.secd

# or, using a bootstrapping interpreter:
$ guile -s scm2secd.scm <scm2secd.scm >scm2secd.secd
$ mzscheme -f scm2secd.scm <scm2secd.scm >scm2secd.secd
```

Scheme expression and files may be evaluated this way:
```bash
$ cat tests/append.scm | ./secd scm2secd.secd | ./secd
```

Then bootstrap REPL: 
```bash
$ ./secd scm2secd.secd <repl.scm >repl.secd
$ ./secd repl.secd
> (append '(1 2 3) '(4 5 6))
(1 2 3 4 5 6)
> (begin (display 'bye) (quit))
bye
$
```

Use `compile` form to examine results of Scheme-to-SECD conversion in the REPL:
```scheme
> (compile '(+ 2 2))
(LDC 2 LDC 2 ADD)
> (eval '(+ 2 2))
4
```

TODO
----
- [ ] refactor out atom_type, reduce size to 4 words instead of 5;
- [ ] '#f';
- [ ] move to indexes instead of pointers;
- [ ] abstract out system memory management: no direct calloc/free references;
- [ ] support for more Scheme types: `bytestring`, `string` (list of chars?), `port`;
- [ ] dot-lists;
- [ ] make symbol storage: quick access (balanced binary search tree or hashtable?), reuse string resources;
- [ ] optional garbage-collector, compare RefCount and GC speed.
- [ ] LLVM backend?
Optimization:
- [ ] fast symbol lookup;
- [ ] remove JOIN/RTN/LD/LDC?

Done:
- [x] `define`, interpreter environment!
- [x] `AP n`: treat n values on top of the stack as arguments for a function call instead of always creating a list.
- [x] don't look up opcodes on its execution;
- [x] tail-recursion/purity analysis on LDF when possible;
