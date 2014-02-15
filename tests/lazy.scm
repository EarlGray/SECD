(define (promise? obj)
    (if (procedure? obj)
        (eq? (car (car obj)) '())
        #f))

(define-macro (delay expr) (list 'lambda '() expr))

(define (force promise) (promise))

(define (delay-force promise) (delay (force promise)))

(define (make-promise obj) (lambda () obj))

