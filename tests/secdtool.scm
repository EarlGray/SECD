(define secdop-info
  '(( #.ADD    0   2 1)
    ( #.AP     0   2 1)
    ( #.CAR    0   1 1)
    ( #.CDR    0   1 1)
    ( #.CONS   0   2 1)
    ( #.DIV    0   2 1)
    ( #.DUM    0   0 0)
    ( #.EQ     0   2 1)
    ( #.JOIN   0   0 0)
    ( #.LD     1   0 1)
    ( #.LDC    1   0 1)
    ( #.LDF    1   0 1)
    ( #.LEQ    0   2 1)
    ( #.MUL    0   2 1)
    ( #.PRINT  0   0 0)
    ( #.RAP    0   2 1)
    ( #.READ   0   0 1)
    ( #.REM    0   2 1)
    ( #.RTN    0   0 0)
    ( #.SEL    2   1 0)
    ( #.STOP   0   0 0)
    ( #.SUB    0   2 1)
    ( #.TYPE   0   1 1)))

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

;; this function does not descend into SEL branches
(define (secd-ctrl-fold func val ctrl)
  (if (null? ctrl)
      val
      (begin
        (define info (info-for (car ctrl)))
        (define to-take (+ 1 (car info)))
        (define oplst (take to-take ctrl))
        (define brest (drop to-take ctrl))
        ;;(display val) (display "\t") (display ctrl) (newline)
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
  (letrec
    ((bv-ht (bound-variables func))
     (save-freevar
       (lambda (ht var) (hashtable-set! ht var (- 0 (hashtable-size ht)))))
     (process-opcode
       (lambda (fv-ht oplst info)
         (begin
           (cond
             ((eq? (car oplst) #.LD)
               ;; check the variable
               (let ((var (cadr oplst)))
                 (let ((mb-val (hashtable-mb-ref fv-ht var)))
                   (if (null? mb-val)
                     (if (not (hashtable-exists? bv-ht var)) (save-freevar fv-ht var) #f)
                     #f))))
             ((eq? (car oplst) #.LDF)
               ;; descend into the lambda recursively
               (let ((subfunc (cadr oplst)))
                  (for-each
                    (lambda (k)
                       (if (hashtable-exists? bv-ht k)
                           #f
                           (if (hashtable-exists? fv-ht k) #f (save-freevar fv-ht k))))
                    (hashtable-keys (free-variables subfunc))))))
         fv-ht))))
   (secd-ctrl-fold process-opcode (make-hashtable) (secd-func-ctrl func))))

