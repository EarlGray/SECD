;;
;; Basic functional things
;;

(define (reverse xs)
  (letrec ((reverse-acc
      (lambda (acc xs)
        (if (null? xs) acc
          (let ((hd (car xs)) (tl (cdr xs)))
               (reverse-acc (cons hd acc) tl))))))
    (reverse-acc '() xs)))

(define (map f xs)
  (letrec ((map-tco
      (lambda (acc xs)
        (if (null? xs)
          acc
          (let ((hd (car xs)) (tl (cdr xs)))
            (map-tco (cons (f hd) acc) tl))))))
    (reverse (map-tco '() xs))))

(define (filter p xs)
  (letrec ((filt-tco
      (lambda (acc xs)
        (if (null? xs)
          acc
          (let ((hd (car xs)) (tl (cdr xs)))
            (if (p hd)
                (filt-tco (cons hd acc) tl)
                (filt-tco acc tl)))))))
    (reverse (filt-tco '() xs))))

(define (foldr f s xs)
  (if (null? xs)
      s
      (f (car xs) (foldr f s (cdr xs)))))
(define (foldl f s xs)
  (if (null? xs)
      s
      (foldl f (f s (car xs)) (cdr xs))))

(define (even x) (eq? (remainder x 2) 0))
(define (odd x)  (eq? (remainder x 2) 1))

(define (range n)
  (letrec ((range-tco
     (lambda (acc n)
        (if (eq? n 0)
            acc
            (range-tco (cons n acc) (- n 1))))))
   (range-tco '() n)))

(define (product xs) (foldr (lambda (x y) (* x y)) 1 xs))
(define (sum xs)     (foldr (lambda (x y) (+ x y)) 0 xs))
