;;
;; Basic functional things
;;

(define (filter p xs)
  (reverse (list-fold (lambda (x lst) (if (p x) (cons x lst) lst)) '() xs)))

(define (foldr f s xs)
  (if (null? xs)
      s
      (f (car xs) (foldr f s (cdr xs)))))
(define (foldl f s xs)
  (if (null? xs)
      s
      (foldl f (f s (car xs)) (cdr xs))))

(define (fold f s . lsts)
  (if (any null? lsts)
      s
      (apply fold (cons f (cons (apply f (cons s (map car lsts))) (map cdr lsts))))))

(define (range n)
  (letrec ((range-tco
     (lambda (acc n)
        (if (eq? n 0)
            acc
            (range-tco (cons n acc) (- n 1))))))
   (range-tco '() n)))

(define (last xs)
  (if (null? xs)
      '()
      (if (null? (cdr xs)) (car xs) (last (cdr xs)))))

(define (take n xs)
  (cond
    ((null? xs) '())
    ((eq? n 0)  '())
    (else (cons (car xs) (take (- n 1) (cdr xs))))))

(define (partition pred xs)
  (let ((res (list-fold
    (lambda (x lst)
      (let ((rights (car lst)) (wrongs (cdr lst)))
        (if (pred x) (cons (cons x rights) wrongs) (cons rights (cons x wrongs)))))
    '(() . ()) xs)))
    (list (reverse (car res)) (reverse (cdr res)))))

(define (remove pred lst)
  (reverse (list-fold (lambda (x res) (if (pred x) res (cons x res))) '() lst)))

(define (any pred lst)
  (call/cc (lambda (return)
    (for-each (lambda (x) (if (pred x) (return #t) #f)) lst))))

(define (every pred lst)
  (call/cc (lambda (return)
    (for-each (lambda (x) (if (pred x) #t (return #f))) lst))))

(define (take-while pred lst)
  (call/cc (lambda (return)
    (reverse (list-fold
                (lambda (x xs) (if (pred x) (cons x xs) (return (reverse xs))))
                '() lst)))))

(define (drop-while pred lst)
  (call/cc (lambda (return)
    (list-fold (lambda (x xs) (if (pred x) (cdr xs) (return xs))) lst lst)))) 

(define (zip . lsts)
  (if (any null? lsts) '()
      (cons (map car lsts) (apply zip (map cdr lsts)))))

(define (even x) (eq? (remainder x 2) 0))
(define (odd x)  (eq? (remainder x 2) 1))

(define (product xs) (foldr (lambda (x y) (* x y)) 1 xs))
(define (sum xs)     (foldr (lambda (x y) (+ x y)) 0 xs))
