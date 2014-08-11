(letrec
;; what:
(
;;
;;  List routines
;;

(list-ref (lambda (xs nth)
  (cond
    ((null? xs)  (raise 'err_out_of_bounds))
    ((eq? nth 0) (car xs))
    (else (list-ref (cdr xs) (- nth 1))))))

(list-tail (lambda (lst nth)
  (cond
    ((eq? nth 0) lst)
    ((null? lst)  (raise 'err_out_of_bounds))
    (else (list-tail (cdr lst) (- nth 1))))))

;; (unzip '((one 1) (two 2) (three 3))) => ((one two three) (1 2 3))
(unzip (lambda (ps)
    (letrec
      ((unzipt
         (lambda (pairs z1 z2)
           (if (null? pairs)
             (list (reverse z1) (reverse z2))
             (let ((pair (car pairs))
                   (rest (cdr pairs)))
               (let ((p1 (car pair))
                     (p2 (cadr pair)))
                 (unzipt rest (cons p1 z1) (cons p2 z2))))))))
      (unzipt ps '() '()))))

(memq (lambda (obj lst)
  (cond
    ((null? lst)         #f)
    ((eq? obj (car lst)) lst)
    (else (memq obj (cdr lst))))))

(assq (lambda (obj alist)
  (cond
    ((null? alist)          #f)
    ((eq? (caar alist) obj) (car alist))
    (else (assq obj (cdr alist))))))

(list-fold (lambda (func val lst)
  (cond
    ((null? lst) val)
    (else (list-fold func (func (car lst) val) (cdr lst))))))

(length (lambda (lst) (list-fold (lambda (x v) (+ 1 v)) 0 lst)))
(reverse (lambda (lst) (list-fold cons '() lst)))
(for-each (lambda (func lst) (list-fold (lambda (x _) (func x)) #f lst)))
(map (lambda (func lst) (reverse (list-fold (lambda (x v) (cons (func x) v)) '() lst))))

(vector-map (lambda (func vect)
  (let ((vectlen (vector-length vect)))
    (letrec
      ((result (make-vector vectlen))
       (iter (lambda (index)
          (if (<= vectlen index)
              result
              (begin
                (vector-set! result index (func (vector-ref vect index)))
                (iter (+ 1 index)))))))
      (iter 0)))))


;;
;;  Scheme to SECD compiler
;;

(compile-bindings
  (lambda (bs)
    (if (null? bs) '(LDC ())
        (append (compile-bindings (cdr bs))
                (secd-compile (car bs))
                '(CONS)))))

(compile-n-bindings
  (lambda (bs)
    (if (null? bs) '()
        (append (compile-n-bindings (cdr bs))
                (secd-compile (car bs))))))

(compile-begin-acc
  (lambda (stmts acc)   ; acc must be '(LDC ()) at the beginning
    (if (null? stmts)
        (append acc '(CAR))
        (compile-begin-acc (cdr stmts)
                           (append acc (secd-compile (car stmts)) '(CONS))))))

(compile-cond
  (lambda (conds)
    (if (null? conds)
        '(LDC ())
        (let ((clause-cond (car (car conds)))
              (clause-body (cdr (car conds))))
          (let ((compiled-body
                  (if (eq? (car clause-body) '=>)
                      (secd-compile (cadr clause-body))
                      (secd-compile (cons 'begin clause-body)))))
            (if (eq? clause-cond 'else)
                compiled-body
                (append (secd-compile clause-cond) '(SEL)
                        (list (append compiled-body '(JOIN)))
                        (list (append (compile-cond (cdr conds)) '(JOIN))))))))))

(compile-quasiquote
  (lambda (lst)
    (cond
      ((null? lst) (list 'LDC '()))
      ((pair? lst)
        (let ((hd (car lst)) (tl (cdr lst)))
           (cond
             ((not (pair? hd))
                (append (compile-quasiquote tl) (list 'LDC hd 'CONS)))
             ((eq? (car hd) 'unquote)
                (append (compile-quasiquote tl) (secd-compile (cadr hd)) '(CONS)))
                ;; TODO: (unquote a1 a2 ...)
             ((eq? (car hd) 'unquote-splicing)
                (display 'Error:_unquote-splicing_TODO)) ;; TODO
             (else (append (compile-quasiquote tl)
                           (compile-quasiquote hd) '(CONS))))))
      (else (list 'LDC lst)))))

(compile-form (lambda (f)
  (let ((hd (car f))
        (tl (cdr f)))
    (cond
      ((eq? hd 'quote)
        (list 'LDC (car tl)))
      ((eq? hd 'quasiquote)
        (compile-quasiquote (car tl)))
      ((eq? hd '+)
        (append (secd-compile (cadr tl)) (secd-compile (car tl)) '(ADD)))
      ((eq? hd '-)
        (append (secd-compile (cadr tl)) (secd-compile (car tl)) '(SUB)))
      ((eq? hd '*)
        (append (secd-compile (cadr tl)) (secd-compile (car tl)) '(MUL)))
      ((eq? hd '/)
        (append (secd-compile (cadr tl)) (secd-compile (car tl)) '(DIV)))
      ((eq? hd 'remainder)
        (append (secd-compile (cadr tl)) (secd-compile (car tl)) '(REM)))
      ((eq? hd '<=)
        (append (secd-compile (cadr tl)) (secd-compile (car tl)) '(LEQ)))
      ((eq? hd 'secd-type)
        (append (secd-compile (car tl)) '(TYPE)))
      ((eq? hd 'car)
        (append (secd-compile (car tl)) '(CAR)))
      ((eq? hd 'cdr)
        (append (secd-compile (car tl)) '(CDR)))
      ((eq? hd 'cons)
        (append (secd-compile (cadr tl)) (secd-compile (car tl)) '(CONS)))
      ((eq? hd 'eq? )
        (append (secd-compile (car tl)) (secd-compile (cadr tl)) '(EQ)))
      ((eq? hd 'if )
        (let ((condc (secd-compile (car tl)))
              (thenb (append (secd-compile (cadr tl)) '(JOIN)))
              (elseb (append (secd-compile (caddr tl)) '(JOIN))))
          (append condc '(SEL) (list thenb) (list elseb))))
      ((eq? hd 'lambda)
        (let ((args (car tl))
              (body (append (secd-compile (cons 'begin (cdr tl))) '(RTN))))
          (list 'LDF (list args body))))
      ((eq? hd 'let)
        (let ((bindings (unzip (car tl)))
              (body (cdr tl)))
          (let ((args (car bindings))
                (exprs (cadr bindings)))
            (append (compile-bindings exprs)
                    (list 'LDF
                        (list args
                            (append (secd-compile (cons 'begin body)) '(RTN))))
                    '(AP)))))
      ((eq? hd 'letrec)
        (let ((bindings (unzip (car tl)))
              (body (cdr tl)))
          (let ((args (car bindings))
                (exprs (cadr bindings)))
              (append '(DUM)
                      (compile-bindings exprs)
                      (list 'LDF
                        (list args
                          (append (secd-compile (cons 'begin body)) '(RTN))))
                      '(RAP)))))

      ;; (begin (e1) (e2) ... (eN)) => LDC () <e1> CONS <e2> CONS ... <eN> CONS CAR
      ((eq? hd 'begin)
        (cond
          ((null? tl) '(LDC ()))                        ;; (begin)
          ((null? (cdr tl)) (secd-compile (car tl)))    ;; (begin (expr)) => (expr)
          ;; TODO: actually, few (define ...) in a row must be rewritten to (letrec* ..)
          ((eq? 'define (caar tl))
            (let ((defform (car tl))
                  (body    (cdr tl)))
              (let ((what (cadr defform))
                    (expr (caddr defform)))
                (cond
                  ((symbol? 'what)
                    (let ((letexpr
                            (list 'let (list `(,what ,expr)) (cons 'begin body))))
                       (secd-compile letexpr)))
                  ;;((pair? 'what) ; TODO: check for let/letrec
                  (else 'Error:_define_what?)))))
          (else (compile-begin-acc tl '(LDC ())))))
      ((eq? hd 'cond)
        (compile-cond tl))
      ((eq? hd 'write)
        (append (secd-compile (car tl)) '(PRINT)))
      ((eq? hd 'read)
        '(READ))
      ((eq? hd 'eval)
        (append '(LDC () LDC ()) (secd-compile (car tl))
           '(CONS LD secd-from-scheme AP AP)))
        ;(secd-compile `((secd-from-scheme ,(car tl)))))
      ((eq? hd 'secd-apply)
        (cond
          ((null? tl) (display 'Error:_secd-apply_requires_args))
          ((null? (cdr tl)) (display 'Error:_secd-apply_requires_second_arg))
          (else (append (secd-compile (car (cdr tl))) (secd-compile (car tl)) '(AP)))))
      ((eq? hd 'call/cc)
        (append (secd-compile (car tl)) '(APCC)))
      ((eq? hd 'quit)
        '(STOP))
      (else
        (let ((macro (lookup-macro hd)))
          (if (null? macro)
              ;; it is a form
              (let ((compiled-head
                      (if (symbol? hd) (list 'LD hd) (secd-compile hd)))
                    (nbinds (length tl)))
                (append (compile-n-bindings tl) compiled-head (list 'AP nbinds)))
              ;; it is a macro application
              (let ((evalclos (secd-apply macro tl)))
                (begin
                  ;(display evalclos)   ;; expanded macro
                  (secd-compile evalclos))))))
    ))))

(secd-compile (lambda (s)
  (cond
    ((pair?   s) (compile-form s))
    ((symbol? s) (list 'LD s))
    (else (list 'LDC s)))))

(secd-compile-top (lambda (s)
    (cond
      ((not (pair? s))
         (secd-compile s))
      ((eq? 'define (car s))
         (let ((what (cadr s))
               (expr (cddr s)))
           (cond
             ((symbol? what)
                (secd-compile (list 'secd-bind! `(quote ,what) (cons 'begin expr))))
             ((pair? what)
                (let ((name (car what))
                      (args (cdr what)))
                  (secd-compile
                    (list 'secd-bind!
                          `(quote ,name)
                          (list 'lambda args (cons 'begin expr))))))
             (else 'Error:_define_what?))))
      (else (secd-compile s)))))

(secd-from-scheme (lambda (s)
    (secd-closure (secd-compile-top s) '() '())))

(secd-closure (lambda (ctrlpath args maybe-env)
  (let ((func (list args (append ctrlpath '(RTN))))
        (env (if (null? maybe-env) (interaction-environment) maybe-env)))
    (cons func env))))

;;  Macros
(lookup-macro (lambda (name)
   (letrec
      ((lookup
        (lambda (macro-list)
          (if (null? macro-list) '()
              (let ((hd (car macro-list)))
                 (let ((macro-name (car hd)))
                   (if (eq? macro-name name) (cdr hd)
                       (lookup (cdr macro-list)))))))))
    (if (symbol? name)
        (lookup *macros*)
        '()))))

(secd-define-macro! (lambda (macrodef macrobody)
  (let ((macroname (car macrodef))
        (macroargs (cdr macrodef)))
    (let ((macroclos ;; TODO: macrobody may be more longer than 1 form
            (secd-closure (secd-compile macrobody) macroargs '())))
      (begin
        ;(display macrobody) ;;; what macro is compiled to.
        (secd-bind! '*macros* (cons (cons macroname macroclos)  *macros*))
        ''ok)))))

;; to be run on SECD only:
(#t (eq? 1 1))
(#f (eq? 1 2))
(not (lambda (b) (if b #f #t)))

(apply (lambda (command arglist) (secd-apply command arglist)))

(+ (lambda (x y) (+ x y)))  ; compiled to (LD x  LD y  ADD)
(- (lambda (x y) (- x y)))
(* (lambda (x y) (* x y)))
(/ (lambda (x y) (/ x y)))
(remainder (lambda (x y) (remainder x y)))
(<= (lambda (x y) (<= x y)))
(car (lambda (pair) (car pair)))
(cdr (lambda (pair) (cdr pair)))
(cons (lambda (hd tl) (cons hd tl)))

(> (lambda (x y) (cond ((eq? x y) #f) (else (<= y x)))))
(< (lambda (x y) (cond ((eq? x y) #f) (else (<= x y)))))

(caar   (lambda (x) (car (car x))))
(cadr   (lambda (x) (car (cdr x))))
(cddr   (lambda (x) (cdr (cdr x))))

(cadar  (lambda (x) (car (cdr (car x)))))
(caddr  (lambda (x) (car (cdr (cdr x)))))
(cdddr  (lambda (x) (cdr (cdr (cdr x)))))
(caddar (lambda (x) (car (cdr (cdr (car x))))))

(vector (lambda args (list->vector args)))

(null?       (lambda (obj) (eq? obj '())))
(pair?       (lambda (obj) (if (null? obj) #f (eq? (secd-type obj) 'cons))))
(number?     (lambda (obj) (eq? (secd-type obj) 'int)))
(symbol?     (lambda (obj) (eq? (secd-type obj) 'sym)))
(string?     (lambda (obj) (eq? (secd-type obj) 'str)))
(vector?     (lambda (obj) (eq? (secd-type obj) 'vect)))
(port?       (lambda (obj) (eq? (secd-type obj) 'port)))
(char?       (lambda (obj) (eq? (secd-type obj) 'char)))
(bytevector? (lambda (obj) (eq? (secd-type obj) 'bvect)))
(procedure?  (lambda (obj)
  (cond
    ((eq? (secd-type obj) 'func)
        #t)
    ((eq? (secd-type obj) 'cons)
      (cond
        ((not (eq? (secd-type (car (cdr obj))) 'frame)) #f)
        ((not (eq? (secd-type (car(car(cdr(car obj))))) 'op)) #f)
        (else
          (let ((args (car (car obj))))
            (cond
              ((null? args) #t)
              (else (eq? (secd-type (car args)) 'sym)))))))
    (else #f))))

(load (lambda (filename)
  (letrec
    ((loadsexp (lambda ()
       (let ((sexpr (read)))
         (if (eof-object? sexpr) 'ok
           (begin
             (eval sexpr (interaction-environment))
             (loadsexp))))))
     (*stdin* (open-input-file filename)))
    (loadsexp))))

(newline (lambda () (display "\n")))

(*secd-exception-handlers*
  (cons (lambda (e) (begin
          (display "** EXCEPTION **\n")
          (display e)
          (display "\n*************\n");
          (repl))) '()))

(with-exception-handler
  (lambda (handler thunk)
    (let ((*secd-exception-handlers*
            (cons handler *secd-exception-handlers*)))
      (thunk))))

(repl (lambda ()
  (begin
    (display *prompt*)
    (let
      ((inp (read)))
     (if (eof-object? inp)
         (quit)
         (let ((result (eval inp (interaction-environment))))
           (begin
             (display "   ")
             (write result)
             (repl))))))))

)

;; <let> in
(begin
  (cond ((not (defined? 'secd))
         (begin
           (display "This file must be run in SECDScheme\n")
           (quit))))
  (secd-bind! '*prompt* "\n;>> ")
  (secd-bind! '*macros*
    (list
      (cons 'define-macro   secd-define-macro!)
      (cons 'box
        (lambda (val) (list 'make-vector 1 val)))
      (cons 'define!
        (lambda (sym val) (list 'secd-bind! `(quote ,sym) `(ref ,val))))
      (cons 'box-set!
        (lambda (sym val) (list 'vector-set! sym 0 val)))
      (cons 'box-ref
        (lambda (sym) (list 'vector-ref sym 0)))))
  (repl)))
