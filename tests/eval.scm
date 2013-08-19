(letrec (
 (repl
    (lambda () (
       (let ((input (read)))
         (if (eof-object? input)
             (display 'bye)
             (begin
               ((make-closure input))
               (display 'done)
               (repl)))))))
) (repl))
