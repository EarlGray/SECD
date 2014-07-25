;;; Short resume of R7RS
; must provide the base library and all exported identifiers

; every other library must be provided in its entirety

;; Identifiers
; support for required characters: !$%&*+-./:<=>?@^_~
;   cases: +soup+, +, <=?, a43kTMNs, ->string, lambda, list->vector
;          ->string, q, V17a, the-word-recursion
; support for zero or more characters inside ||
;   cases: ||, |two words|, |two\x20;words|
;          (symbol=? |two words| |two\x20;words|) => #t
; whitespaces: ' ', '\t', '\n' + maybe more
; comments:
;   ;<any><end-of-line>,
;   #;<maybe whitespace><datum>
;   #|<multiline block comment>|#
; datum label: #<n>=<datum>, #<n>#
; disjointness of types:
;   boolean? bytevector? char? eof-object? null? number?
;   pair? port? procedure? string? symbol? vector?
;   + all created by define-record-type
; external representation:
;   "28", "#e28.000", "#x1c" are ER of the same object; (+ 8 20) is not.
;   not all types have ER
;   (read), (write)
; storage model:
;   '() is unique
;   "", #(), #u8() may be or may be not newly allocated
;   immutable: literal constants, results of symbol->string, scheme-report-environment
; proper tail recursion
;   (lambda <formals> <defines>* <expr>* <tailexpr>)
;   (if <expr> <tailexpr> <tailexpr>)
;   (cond (<test> <tailseq>)* (else <tailseq>)?)
;   (case ((<datum>+) <tailseq>)* (else <tailseq>)?)
;   (and <expr>* <tailexpr>) (or <expr>* <tailexpr>)
;   (when <test> <tailseq>)  (unless <test> <tailseq>)
;   let, let*, letrec, letrec*, let-values, let-values*
;   let-syntax, letrec-syntax
;   (begin <tailseq>)
; expressions
;   <variable>
;   literal expressions: (quote <datum>), <const>
;   numerical, boolean, string, char, bytevector: self-evaluating
;        '# => #, # => #, ''a => (quote a), '"abc" => "abc"
; function calls
;   all operands are evaluated (in unspecified order)
;       ((if #f + -) 4 2) => 2
;   multiple return values with (values ...)
;   (apply fun args)

; procedures
;   (lambda <formals> <body>) => obj, s.t. (procedure? obj), closure
;   ((lambda (x) (+ x x)) 4)  => 8
;   formals: (arg*), arglist, (arg* . argrest)
;     arg must be unique
; conditions:
;   (if <test> <consequent> <alternate>)
;   (if <test> <consequent>)
;       (if #t <consequent>) => <unspecified>
; assignments
;   (set! <var> <expr>) => <unspecified>
;     <var> must be bound
; inclusions
;   (include "<filename>"+),
;   => (let ((content (read)+)) (begin content))
;   (include-ci "<filename>"+) - as if with #!fold-case
; conditionals
;   (cond <clause>+ (else =>? <expr>+)?) 
;       where <clause> ::= (<test> <expr>*) | (<test> => <expr>)
;       (cond (<test>) ...) if <test> => <test>
;       cond with no <test_i> => <unspecified>
;       (cond), (cond (else #f) (#t)) are errors
;   (case <key> <clause>+ <else-clause>?)
;       where <clause> ::= ((<datum>+) =>? <expr>*), <else-clause> ::= (else =>? <expr>)
;       (eqv? ...) equality
;       if no <clause> matches and there's no <else-clause> => <unspecified>
;   (and <test1>*) => #f if any <testI> is #f; left-to-right short-circuited evaluation
;                  => value of the last <test> otherwise, (and) => #t
;   (or <test1>*)  => value of the first expr that is #t
;                  => #f otherwise, (or) => #f
;   (when|unless <test> <expr>+) => <unspecified>
;   (cond-expand <ce-clause>*) - first <ce-clause> that is #t, is evaluated
;       where <ce-clause> ::= (<featurereq> <expr>+)
;             <featurereq> ::= <featureid> | (library <name>) 
;                            | (and <featurereq>+) | (or <featurereq>+)
;                            | (not <featurereq>
; bindings
