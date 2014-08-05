(letrec (
 (repl
    (lambda () (
       (let ((input (read)))
         (if (eof-object? input)
             (display 'bye)
             (begin
               ((secd-closure (secd-compile input) '() '()))
               (display 'done)
               (repl)))))))
) (repl))
