(define (quicksort xs)
  (cond
    ((null? xs) xs)
    ((null? (cdr xs)) xs)
    (else
      (let ((pivot (car xs)))
        (let ((r (partition (lambda (x) (<= x pivot)) (cdr xs))))
          (append (quicksort (car r))
                  (cons pivot
                        (quicksort (cadr r)))))))))
