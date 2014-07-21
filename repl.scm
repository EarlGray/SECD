(letrec
;; what:
(
(#t (eq? 1 1))
(#f (eq? 1 2))
(secd-not (lambda (b) (if b #f #t)))

(length (lambda (xs)
  (letrec
    ((len (lambda (xs acc)
            (if (null? xs) acc
                (len (cdr xs) (+ 1 acc))))))
    (len xs 0))))

(unzip (lambda (ps)
    (letrec
      ((unzipt
         (lambda (pairs z1 z2)
           (if (null? pairs)
             (list z1 z2)
             (let ((pair (car pairs))
                   (rest (cdr pairs)))
               (let ((p1 (car pair))
                     (p2 (cadr pair)))
                 (unzipt rest (append z1 (list p1)) (append z2 (list p2)))))))))
      (unzipt ps '() '()))))

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
        (let ((this-cond (car (car conds)))
              (this-expr (cadr (car conds))))
          (if (eq? this-cond 'else)
              (secd-compile this-expr)
              (append (secd-compile this-cond) '(SEL)
                      (list (append (secd-compile this-expr) '(JOIN)))
                      (list (append (compile-cond (cdr conds)) '(JOIN)))))))))

(compile-quasiquote
  (lambda (lst)
    (cond
      ((null? lst) (list 'LDC '()))
      ((pair? lst)
        (let ((hd (car lst)) (tl (cdr lst)))
           (cond
             ((secd-not (pair? hd))
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
      ((eq? hd 'pair?)
        (append (secd-compile (car tl)) '(TYPE LDC cons EQ)))
      ((eq? hd 'car)
        (append (secd-compile (car tl)) '(CAR)))
      ((eq? hd 'cdr)
        (append (secd-compile (car tl)) '(CDR)))
      ((eq? hd 'cadr)
        (append (secd-compile (car tl)) '(CDR CAR)))
      ((eq? hd 'caddr)
        (append (secd-compile (car tl)) '(CDR CDR CAR)))
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
              (body (append (secd-compile (cadr tl)) '(RTN))))
          (list 'LDF (list args body))))
      ((eq? hd 'let)
        (let ((bindings (unzip (car tl)))
              (body (cadr tl)))
          (let ((args (car bindings))
                (exprs (cadr bindings)))
            (append (compile-bindings exprs)
                    (list 'LDF (list args (append (secd-compile body) '(RTN))))
                    '(AP)))))
      ((eq? hd 'letrec)
        (let ((bindings (unzip (car tl)))
              (body (cadr tl)))
          (let ((args (car bindings))
                (exprs (cadr bindings)))
              (append '(DUM)
                      (compile-bindings exprs)
                      (list 'LDF (list args (append (secd-compile body) '(RTN))))
                      '(RAP)))))

      ;; (begin (e1) (e2) ... (eN)) => LDC () <e1> CONS <e2> CONS ... <eN> CONS CAR
      ((eq? hd 'begin)
        (compile-begin-acc tl '(LDC ())))
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

(secd-make-executable (lambda (ctrlpath maybe-env)
  (let ((func (list '() (append ctrlpath '(RTN))))
        (env (if (null? maybe-env) (interaction-environment) maybe-env)))
    (cons func env))))

(secd-closure (lambda (ctrlpath args maybe-env)
  (let ((func (list args (append ctrlpath '(RTN))))
        (env (if (null? maybe-env) (interaction-environment) maybe-env)))
    (cons func env))))

(lookup-macro (lambda (name)
   (letrec
      ((lookup
        (lambda (*macro-list*)
          (if (null? *macro-list*) '()
              (let ((hd (car *macro-list*)))
                 (let ((macro-name (car hd)))
                   (if (eq? macro-name name) (cdr hd)
                       (lookup (cdr *macro-list*)))))))))
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

(secd-mdefine! (lambda (definition initval)
  (if (symbol? definition)
      (list 'secd-bind! `(quote ,definition) initval)
      ;; a function definition:
      (let ((name (car definition)) (args (cdr definition)))
           (list 'secd-bind! `(quote ,name)
                 (list 'lambda args initval))))))

(secd-from-scheme (lambda (s)
    (secd-make-executable (secd-compile s) '())))


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

(repl (lambda ()
  (begin
    (display *prompt*)
    (let ((inp (read)))
      (if (eof-object? inp) (quit)
        (let ((result (eval inp (interaction-environment))))
         (begin
          (display "   ")
          (write result)
          (repl))))))))

;; to be run on SECD only:
(apply (lambda (command arglist) (secd-apply command arglist)))

(null?       (lambda (obj) (eq? obj '())))
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
        ((secd-not (eq? (secd-type (car (cdr obj))) 'frame)) #f)
        ((secd-not (eq? (secd-type (car(car(cdr(car obj))))) 'op)) #f)
        (else
          (let ((args (car (car obj))))
            (cond
              ((null? args) #t)
              ((secd-not (eq? (secd-type (car args)) 'sym)) #f)
              (else #t))))))
    (else #f))))

)
 
;; <let> in
(begin
  (cond 
    ((defined? 'secd)
      '())
    (else
      (begin
        (display "This file must be run in SECDScheme\n")
        (quit))))
  (secd-bind! '*prompt* "\n;>> ")
  (secd-bind! '*macros*
    (list
      (cons 'define-macro   secd-define-macro!)
      (cons 'define         secd-mdefine!)))
  (repl)))
