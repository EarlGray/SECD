(define (make-hashtable)
  (let ((initial-capacity 16))
    (let ((lookup-table (make-vector initial-capacity -1)))
      (list->vector (list initial-capacity lookup-table 0)))))

(define (hashtable-index hash cap)
  (let ((ind (remainder hash cap)))
    (cond
      ((eq? ind 0) ind)
      ((<= ind 0) (+ ind cap))
      (else ind))))


(define (alist-lookup alist key)
  (if (null? alist)
      '()
      (let ((kv (car alist)))
        (if (eq? (car kv) key)
            (list (cdr kv))
            (alist-lookup (cdr alist) key)))))

(define (alist-replace alist key value)
  (letrec ((replace-tco
      (lambda (rest)
        (if (null? rest) '()
          (let ((kv (car rest)) (rst (cdr rest)))
            (if (eq? (car kv) key)
                (cons (cons key value) rst)
                (cons kv (replace-tco rst))))))))
    (replace-tco alist)))

;; accepts symbols only at the moment
(define (hashtable-set! hashtable sym val)
  (let ((capacity (vector-ref hashtable 0))
        (table    (vector-ref hashtable 1))
        (count    (vector-ref hashtable 2)))
    (let ((index (hashtable-index (secd-hash sym) capacity)))
      (let ((prev-alist (vector-ref table index)))
        (if (pair? prev-alist)
          (vector-set! table index (alist-replace prev-alist sym val))
          (begin
            (vector-set! table  index  (list (cons sym val)))
            (vector-set! hashtable 2 (+ (vector-ref hashtable 2) 1))))))))

;; returns '() if no value
;; returns (value) if key has value
(define (hashtable-ref hashtable key)
  (let ((capacity (vector-ref hashtable 0))
        (table    (vector-ref hashtable 1))
        (count    (vector-ref hashtable 2)))
    (let ((index (hashtable-index (secd-hash key) capacity)))
      (let ((alist (vector-ref table index)))
        (if (pair? alist)
           (alist-lookup alist key)
           '())))))
        
(define (rebalance-hashtable ht) ht)
