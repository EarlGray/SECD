SECDScheme
==========

This is a loose implementation of [SECD machine](http://en.wikipedia.org/wiki/SECD) and a simple self-hosted Scheme-to-SECD compiler/interpreter.

Running Scheme:
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

;>> (load "std/lists.scm")
   ok

;>> (filter odd (range 12))
   (1 3 5 7 9 11)

;>> (begin (display 'bye) (quit))
bye
$
```

Running bare [SECD opcodes](docs/SECD.md):
```
$ echo "(STOP)" | ./secd
$ echo "(LDC 2  LDC 2  ADD  PRINT  STOP)" | ./secd
$ ./secd < tests/append.secd
(1 2 3 4 5 6)
```

The design is mostly inspired by detailed description in _Functional programming: Application and Implementation_ by Peter Henderson and his LispKit, but is not limited by the specific details of traditional SECD implementations (like 64 Kb size of heap, etc) and R7RS.

Here is [a series of my blog posts about SECD machine](http://dmytrish.wordpress.com/2013/08/09/secd-about)

Join a [Gitter chat](https://gitter.im/EarlGray/SECD) if you want to discuss the project or need help with it.
