(define (string->number s)
  (let ((*stdin* (open-input-string s)))
    (let ((num (read-lexeme)))
      (if (eq? 'int (car (cdddr num)))
          (if (eof-object? (read-lexeme))
              (car (list-tail num 4))
              #f)
          #f))))

(define (lexdebug s)
  (let ((*stdin* (open-input-string s)))
    (let loop ((endchr #\space) (line 1) (pos 0))
      (let ((inp (read-lexeme endchr line pos)))
        (if (eof-object? inp) 'ok
            (begin
              (display line) (display ":") (display pos) (display ": ")
              (display inp) (newline)
              (loop (car (list-tail inp 2)) (car inp) (cadr inp))))))))
