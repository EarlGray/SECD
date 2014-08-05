(define (andfunc . thunks)
  (if (null? thunks) #t
      (if ((car thunks)) (apply andfunc (cdr thunks)) #f)))

(define (orfunc . thunks)
  (if (null? thunks) #f
      (let ((res ((car thunks)))
        (if res res (apply orfunc (cdr thunks)))))))

(define-macro (and . exprs)
  (let ((thunks (map (lambda (expr) `(lambda () ,expr)) exprs)))
    `(apply andfunc (list ,@thunks))))

(define-macro (or . exprs)
  (let ((thunks (map (lambda (expr) `(lambda () ,expr)) exprs)))
    `(apply orfunc (list ,@thunks))))
