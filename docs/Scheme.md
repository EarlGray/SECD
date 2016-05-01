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
