(letrec
;; what:
(
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

(length (lambda (xs)
  (letrec 
    ((len (lambda (xs acc)
            (if (null? xs) acc
                (len (cdr xs) (+ 1 acc))))))
    (len xs 0))))

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

(atom? (lambda (b) (not (pair? b))))

(compile-quasiquote
  (lambda (lst)
    (if (null? lst) '()
        (let ((hd (car lst)) (tl (cdr lst)))
          (if (atom? hd)
              (append (compile-quasiquote tl) (list 'LDC hd 'CONS))
              (cond
                ((eq? (car hd) 'unquote)
                   (append (compile-quasiquote tl) (secd-compile (cadr hd)) '(CONS)))
                   ;; TODO: (unquote a1 a2 ...)
                ((eq? (car hd) 'unquote-splicing)
                   (error 'TODO)) ;; TODO
                (else (append (compile-bindings tl) (list 'LDC hd 'CONS)))))))))

(compile-form (lambda (f)
  (let ((hd (car f))
        (tl (cdr f)))
    (cond
      ((eq? hd 'quote)
        (list 'LDC (car tl)))
      ((eq? hd 'quasiquote)
        (append '(LDC ()) (compile-quasiquote (car tl))))
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
      ((eq? hd 'atom?)
        (append (secd-compile (car tl)) '(ATOM)))
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
      ((eq? hd 'display)
        (append (secd-compile (car tl)) '(PRINT)))
      ((eq? hd 'read)
        '(READ))
      ((eq? hd 'eval)
        (append '(LDC () LDC ()) (secd-compile (car tl)) '(CONS LD secd-from-scheme AP AP)))
      ((eq? hd 'quit)
        '(STOP))
      (else
        (let ((compiled-head
                (if (symbol? hd) (list 'LD hd) (secd-compile hd)))
              (nbinds (length tl)))
         (append (compile-n-bindings tl) compiled-head (list 'AP nbinds))))
    ))))

(secd-compile (lambda (s)
  (cond
    ((symbol? s) (list 'LD s))
    ((number? s) (list 'LDC s))
    (else (compile-form s)))))

(secd-closure (lambda (ctrlpath maybe-env)
  (let ((func (list '() (append ctrlpath '(RTN))))
        (env (if (null? maybe-env) (interaction-environment) maybe-env)))
    (cons func env))))

(secd-from-scheme (lambda (s)
    (secd-closure (secd-compile s) nil)))

(repl (lambda (env)
    (let ((inp (read)))
      (if (eof-object? inp) (quit)
        (begin
          (display (eval inp env))  ;; compile and run
          (repl env))))))
)

;; <let> in
(repl (interaction-environment)))
