(define secdop-info
  '(( #.ADD    0   2 1 ADD)
    ( #.AP     0   2 1 AP)
    ( #.CAR    0   1 1 CAR)
    ( #.CDR    0   1 1 CDR)
    ( #.CONS   0   2 1 CONS)
    ( #.DIV    0   2 1 DIV)
    ( #.DUM    0   0 0 DUM)
    ( #.EQ     0   2 1 EQ)
    ( #.JOIN   0   0 0 JOIN)
    ( #.LD     1   0 1 LD)
    ( #.LDC    1   0 1 LDC)
    ( #.LDF    1   0 1 LDF)
    ( #.LEQ    0   2 1 LEQ)
    ( #.MUL    0   2 1 MUL)
    ( #.PRINT  0   0 0 PRINT)
    ( #.RAP    0   2 1 RAP)
    ( #.READ   0   0 1 READ)
    ( #.REM    0   2 1 REM)
    ( #.RTN    0   0 0 RTN)
    ( #.SEL    2   1 0 SEL)
    ( #.STOP   0   0 0 STOP)
    ( #.SUB    0   2 1 SUB)
    ( #.TYPE   0   1 1 TYPE)))

(define (takes-from-stack info) (cadr info))
(define (puts-to-stack info)    (caddr info))

(define (closure-func clos) (car clos))
(define (secd-func-ctrl func) (cadr func))
(define (secd-func-args func) (car func))
(define (closure-ctrl clos) (secd-func-ctrl (closure-func clos)))

(define (info-for op)
  (letrec ((search
             (lambda (info)
               (if (null? info)
                   'err:_no_info_for
                   (if (eq? op (car (car info)))
                     (cdr (car info))
                     (search (cdr info)))))))
    (search secdop-info)))

(define (take n lst)
  (if (eq? n 0) '()
      (if (null? lst) '()
          (cons (car lst) (take (- n 1) (cdr lst))))))
(define (drop n lst)
  (if (eq? n 0) lst
      (if (null? lst) '()
          (drop (- n 1) (cdr lst)))))

;(define (secd-ctrl-compile ctrl compiled)
;  (cond
;    ((null? ctrl) (reverse compiled))
;    ((or (eq? (car ctrl) 'SEL) (eq? (car ctrl) #.SEL))
;      (let ((thenb (secd-ctrl-compile (cadr ctrl) '()))
;            (elseb (secd-ctrl-compile (caddr ctrl) '()))
;            (joinb (cadr (cddr ctrl))))
;        (secd-ctrl-compile joinb (cons elseb (cons thenb (cons #.SEL compiled))))))
;    ((or (eq? (car ctrl) 'LDF) (eq? (car ctrl) #.LDF))
;      (let ((subfunc (cadr ctrl)))
;        (let ((subargs (car subfunc))
;              (subctrl (secd-ctrl-compile (cadr subfunc) '()))
;              (subother (cddr subfunc))))
;          (secd-ctrl-compile (caddr ctrl)
;             (cons (cons subargs (cons subctrl subother)) (cons #.LDF compiled)))))
;    (else
;      (cond
;        ((eq? (secd-type (car ctrl)) 'op)
;          (let ((info (info-for (car ctrl))))
;            (secd-ctrl-compile (list-tail ctrl (car info))
;
;; this function does not descend into SEL branches
(define (secd-ctrl-fold func val ctrl)
  (if (null? ctrl)
      val
      (begin
        (define info (info-for (car ctrl)))
        (define to-take (+ 1 (car info)))
        (define oplst (take to-take ctrl))
        (define brest (drop to-take ctrl))
        (cond
          ((eq? (car oplst) #.AP)
            (if (number? (cadr ctrl))
                (secd-ctrl-fold func (func val (take 2 ctrl) info) (cddr ctrl))
                (secd-ctrl-fold func (func val oplst info) brest)))
          (else (secd-ctrl-fold func (func val oplst info) brest))))))

(define (secd-stack-depth ctrl)
  (letrec
    ((iteration
      (lambda (depth oplst info)
        (if (number? depth)
            (begin
              (define from-stack (if (eq? (car oplst) #.AP)
                                     (if (null? (cdr oplst)) 2 (+ 1 (cadr oplst)))
                                     (takes-from-stack info)))
              (define to-stack   (puts-to-stack info))
              (define depth1     (- depth from-stack))
              (define depth2     (+ depth1 to-stack))
              (cond
                ((> 0 depth1) 'error:_stack_underflow)
                ((eq? (car oplst) #.SEL)
                  (begin
                    (define thendepth
                              (secd-ctrl-fold iteration depth2 (cadr oplst)))
                    (define elsedepth
                              (secd-ctrl-fold iteration depth2 (caddr oplst)))
                    (if (eq? thendepth elsedepth)
                        thendepth
                        'err:_then_else_disbalance)))
                (else depth2)))
            depth))))
    (secd-ctrl-fold iteration 0 ctrl)))

(define (valid-stack clos)
    (eq? 1 (secd-stack-depth (closure-ctrl clos))))

(define (bound-variables func)
  (let ((ht (make-hashtable)))
    (begin   ;; using (hashtable-size as counter:
      (for-each (lambda (arg) (hashtable-set! ht arg (hashtable-size ht))) (secd-func-args func))
      ht)))

;; takes a compiled function definition (func part of a closure)
;; returns hashtable with FV set as keys
(define (free-variables func)
  (let ((bv-ht (bound-variables func))
        (dumrap (vector #f #f)))
    (letrec
      ((save-freevar
         (lambda (ht var)
           (cond
             ((hashtable-exists? bv-ht var) #f)
             ((hashtable-exists? ht var)    #f)
             (else
               (let ((dr-vars (vector-ref dumrap 0)))
                 (let ((the-ht (if dr-vars dr-vars ht)))
                   (hashtable-set! the-ht var (- -1 (hashtable-size the-ht)))))))))
       (process-opcode
         (lambda (fv-ht oplst info)
           (begin
             (display ";; process-opcode: ") (display oplst) (newline)
             (cond
               ((eq? (car oplst) #.LD)
                 (save-freevar fv-ht (cadr oplst)))
               ((eq? (car oplst) #.SEL)
                 (begin
                   (secd-ctrl-fold process-opcode fv-ht (cadr oplst))
                   (secd-ctrl-fold process-opcode fv-ht (caddr oplst))))
               ((eq? (car oplst) #.LDF)
                 ;; descend into the lambda recursively
                 (let ((subfunc (cadr oplst)))
                    (cond ((vector-ref dumrap 0)
                             (vector-set! dumrap 1 subfunc)))
                    (for-each
                      (lambda (k) (save-freevar fv-ht k))
                      (hashtable-keys (free-variables subfunc)))))
               ;((eq? (car oplst) #.DUM)
               ;  (display ";; free-variables: DUM\n")
               ;  (vector-set! dumrap 0 (make-hashtable)))
               ;((eq? (car oplst) #.RAP)
               ;  (let ((dr-vars (vector-ref dumrap 0))
               ;        (dr-bv (bound-variables (vector-ref dumrap 1))))
               ;    (display ";; free-variables: RAP\n")
               ;    (vector-set! dumrap 0 #f)
               ;    (for-each
               ;      (lambda (k)
               ;        (if (hashtable-exists? dr-bv k) #f (save-freevar fv-ht k)))
               ;      (hashtable-keys dr-vars)))))
               )
           fv-ht))))
     (secd-ctrl-fold process-opcode (make-hashtable) (secd-func-ctrl func)))))

