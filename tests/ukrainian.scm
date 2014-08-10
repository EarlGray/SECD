;; This is a half-humorous exercise to translate Scheme core into
;; Ukrainian. It is not supposed to be used for anything useful, 
;; I just can't resist temptation to see limits of Ukrainian 
;; (in)expressiveness as a programming language.

;; Also this file is a nice stress test for heavy macro usage

(define-macro (означ-макрос що чим) (list 'define-macro що чим))

(означ-макрос (означ що чим)
  (cond
    ((pair? що)
      (let ((name (car що)) (args (cdr що)))
        (list 'secd-bind! `(quote ,name) (list 'lambda args чим))))
    ((symbol? що)
      (list 'secd-bind! `(quote ,що) чим))))

(означ-макрос (список . елементи) (cons 'list елементи))

(означ-макрос (цит що)      (list 'quote що))
(означ-макрос (квцит що)    (list 'quasiquote що))

(означ-макрос (виконай . що) (cons 'eval що))
(означ-макрос (виклик чого аргументи) (list 'secd-apply чого аргументи))
(означ-макрос (вихід)       '(quit))
;; виклик-з-поточним-продовженням:
(означ-макрос (викл-пп дія)  (cons 'call/cc дія))

;; списки
;;   cons - КЛіТинка
(означ-макрос (клт голова хвіст) `(cons ,голова ,хвіст))
;;   car - ПеРШе зі списку
(означ-макрос (прш клт)          `(car ,клт))
;;   cdr - РеШТа списку
(означ-макрос (ршт клт)          `(cdr ,клт))

;; два синоніми для lambda
(означ-макрос (функція аргументи . тіло)
    (cons 'lambda (cons аргументи тіло)))
(означ-макрос (дія аргументи . тіло)
    (cons 'lambda (cons аргументи тіло)))

(означ (переписати-означення вирази)
  (map
    (lambda (expr)
      (cond
        ((pair? expr)
          (if (eq? (car expr) 'означ) (cons 'define (cdr expr)) expr))
        (else expr)))
    вирази))

(означ-макрос (блок . тіло)      (cons 'begin (переписати-означення тіло)))
(означ-макрос (якщо умова то інакше)
    (list 'if умова то інакше))
(означ-макрос (умовно . гілки)   (cons 'cond гілки))

(означ-макрос (введи)           '(read))
(означ-макрос (виведи що)       (list 'write що))
(означ-макрос (друк . що)       (cons 'display що))

(означ-макрос (хай змінні . вирази)
    (cons 'let (cons змінні (переписати-означення вирази))))
(означ-макрос (нехай змінні . вирази)
    (cons 'letrec (cons змінні (переписати-означення вирази))))

(означ-макрос (екв? перше друге)   (list 'eq? перше друге))

(означ-макрос (залишок ділене дільник) (list 'remainder ділене дільник))

(означ Так #t)
(означ Ні  #f)
(означ-макрос (не вираз) (list 'not вираз))

(означ-макрос (по-списку дія список)  (list 'for-each дія список))

(означ унарні-функції
  '((пусте?     . null?)
    (пара?      . pair?)
    (число?     . number?)
    (символ?    . symbol?)))

;(по-списку
;  (дія (пара)
;    (хай ((переклад (прш пара)) (оригінал (ршт пара)))
;      (означ-макрос (переклад аргумент) (list оригінал аргумент))))
;  унарні-функції)
