;; creates a new empty hashtable
(define (make-hashtable)
  (let ((initial-capacity 2))
    (let ((lookup-table (make-vector initial-capacity 0)))
      (list->vector (list initial-capacity lookup-table 0)))))

;; return count of mappings
(define (hashtable-size ht) (vector-ref ht 2))

(define (hashtable-capacity ht) (vector-ref ht 0))
(define (hashtable-loadratio ht)
   (/ (* 100 (vector-ref ht 2)) (vector-ref ht 0)))

;; for internal use
(define (hashtable-index key cap)
  (let ((ind (remainder (secd-hash key) cap)))
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

;; accepts symbol keys only at the moment
(define (hashtable-set! hashtable key val)
  (let ((capacity (vector-ref hashtable 0))
        (table    (vector-ref hashtable 1))
        (count    (vector-ref hashtable 2)))
    (let ((index (hashtable-index key capacity))
          (inc-count (lambda (h) (vector-set! h 2 (+ (hashtable-size h) 1)))))
      (let ((prev-alist (vector-ref table index)))
        (begin
          (if (pair? prev-alist)
            (letrec
              ((alist-replace
                 (lambda (alist)
                   (if (null? alist)
                       (begin (inc-count hashtable) (list (cons key val)))
                       (let ((kv (car alist)) (rst (cdr alist)))
                          (if (eq? (car kv) key)
                              (cons (cons key val) rst)
                              (cons kv (alist-replace rst))))))))
              (vector-set! table index (alist-replace prev-alist key val)))
            (begin
              (inc-count hashtable)
              (vector-set! table index (list (cons key val)))))
          (if (<= (hashtable-loadratio hashtable) 75)
              hashtable
              (let ((reb-ht (rebalanced-hashtable hashtable)))
                (begin
                  (vector-set! hashtable 0 (vector-ref reb-ht 0))
                  (vector-set! hashtable 1 (vector-ref reb-ht 1))
                  (vector-set! hashtable 2 (vector-ref reb-ht 2))
                  hashtable))))))))

;; returns '() if no value
;; returns (value) if key has value
(define (hashtable-mb-ref hashtable key)
  (let ((capacity (vector-ref hashtable 0))
        (table    (vector-ref hashtable 1))
        (count    (vector-ref hashtable 2)))
    (let ((index (hashtable-index key capacity)))
      (let ((alist (vector-ref table index)))
        (if (pair? alist)
           (alist-lookup alist key)
           '())))))

(define (hashtable-ref/default hashtable key default)
   (let ((mb-val (hashtable-mb-ref hashtable key)))
     (if (null? mb-val)
         default
         (car mb-val))))

(define (hashtable-ref hashtable key)
  (car (hashtable-mb-ref hashtable key)))

(define (hashtable-exists? hashtable key)
  (not (null? (hashtable-mb-ref hashtable key))))

(define (alist->hashtable alist)
  (let ((ht (make-hashtable)))
    (begin
      (for-each
        (lambda (pair)
          (let ((key (car pair)) (val (cdr pair)))
            (hashtable-set! ht key val)))
        alist)
      ht)))

(define (hashtable-merge! this that)
  (begin
    (for-each
      (lambda (key)
        (cond ((not (hashtable-exists? this key)) 
               (hashtable-set! this key (hashtable-ref that key)))))
      (hashtable-keys that))
    this))

(define (hashtable-fold func state hashtable)
  (let ((htlen (hashtable-capacity hashtable)))
    (letrec
       ((folditer (lambda (htpair state)
                    (let ((key (car htpair)) (val (cdr htpair)))
                      (func key val state))))
        (fold-htentry (lambda (index state)
          (if (<= htlen index)
              state
              (let ((htalist (vector-ref (vector-ref hashtable 1) index)))
                (fold-htentry (+ 1 index)
                  (if (pair? htalist) (list-fold folditer state htalist) state)))))))
      (fold-htentry 0 state))))


(define (hashtable-keys hashtable)
  (hashtable-fold
    (lambda (key val state) (cons key state))
    '()
    hashtable))

(define (hashtable-copy that)
  (hashtable-merge! (make-hashtable) that))

(define (rebalanced-hashtable ht)
  (let ((old-cap   (vector-ref ht 0))
        (old-table (vector-ref ht 1))
        (count     (vector-ref ht 2)))
    (let ((new-cap (* 2 old-cap)))
      (let ((new-table (make-vector new-cap 0))
            (inc-count (lambda (h) (vector-set! h 2 (+ 1 (hashtable-size h))))))
        (letrec
            ((new-ht (list->vector (list new-cap new-table 0)))
             (while-not-zero        ;; rewrite with (vector-map)
                (lambda (i f)
                   (if (eq? i 0)
                       (f 0)
                       (begin (f i) (while-not-zero (- i 1) f)))))
             (map-alist
               (lambda (f alist)
                 (if (null? alist) '()
                     (cons (f (car alist)) (map-alist f (cdr alist)))))))
         (begin
           (while-not-zero (- old-cap 1)
             (lambda (i)
               (let ((alist (vector-ref old-table i)))
                 (if (pair? alist)
                     (map-alist (lambda (kv) (hashtable-set! new-ht (car kv) (cdr kv))) alist)
                     '()))))
           new-ht))))))
