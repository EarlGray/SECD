(define (test-dynwind)
  (let ((msgs '()) (c #f))
    (let ((log (lambda (msg) (set! msgs (cons msg msgs)))))
      (dynamic-wind
        (lambda () (log 'connect))
        (lambda ()
          (log (call/cc (lambda (k) 
                 (set! c k)
                 'first-time-here))))
        (lambda () (log 'disconnect)))
      (if (< (length msgs) 4)
          (c 'second-time)
          (reverse msgs)))))

(define-macro (box val) `(make-vector 1 ,val))
(define-macro (box-set! b val) `(vector-set! ,b 0 ,val))
(define-macro (box-ref b) `(vector-ref ,b 0))

(define kont (box #f))
(define (test-dynwind2)
  (dynamic-wind
    (lambda () (display "entered test2 dynextent\n"))
    (lambda ()
      (dynamic-wind
        (lambda () (display "entered test2inner dynextent\n"))
        (lambda () (call/cc (lambda (k) 
            (if (box-ref kont) ((box-ref kont) 12) (box-set! kont k)))))
        (lambda () (display "exited test2inner dynextent\n"))))
    (lambda () (display "exited test2 dynextent\n"))))

(define (test-dynwind3)
 (dynamic-wind
   (lambda () (display "test3 outer entered"))
   (lambda ()
     (let ((save-context (lambda (x) 
         (dynamic-wind
            (lambda () (display "test3 save-context entered\n"))
            (lambda ()
                (call/cc (lambda (k)
                   (if (box-ref kont) ((box-ref kont) 12) (box-set! kont k)) x)))
            (lambda () (display "test3 save-context exited\n"))))))
        (save-context 21))
     (dynamic-wind
       (lambda () (display "test3 inner entered\n"))
       (lambda () ((box-ref kont) 15))
       (lambda () (display "test3 inner entered\n"))))
   (lambda () (display "test3 outer exited\n"))))
