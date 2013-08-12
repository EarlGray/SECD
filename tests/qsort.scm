(letrec (
  (not (lambda (x)
    (if x '() #t)))
  (and (lambda (x y)
    (if x y '())))
  (< (lambda (x y)
    (if (eq? x y) '()
        (<= x y))))

  (length (lambda (xs)
    (if (eq? xs '()) 0
        (+ 1 (length (cdr xs))))))

  (unzip-with (lambda (pred xs)
    (if (eq? xs '())
        (list '() '())
        (let ((hd (car xs))
              (r (unzip-with pred (cdr xs))))
          (let ((r-pos (car r))
                (r-neg (cadr r)))
            (if (pred hd)
                (list (cons hd r-pos) r-neg)
                (list r-pos (cons hd r-neg))))))))

  (quicksort
    (lambda (xs)
      (let ((n (length xs)))
        (if (<= n 1) xs
            (let ((pivot (car xs)))
               (let ((r (unzip-with (lambda (x) (<= x pivot)) (cdr xs))))
                  (append (quicksort (car r))
                          (cons pivot
                                (quicksort (cadr r))))))))))
  )
  (let
    ((inp (read)))
    ;; for testing:
    ;((inp '(50 12 32 87 13 45 64 31 11 99 73)))
      (display (quicksort inp))
    ))
