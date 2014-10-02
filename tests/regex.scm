;;
;; regular expressions
;;

(define EPSILON #xFF)

(define (make-dynamic-vector)
  (let ((FREE_INDEX 0)
        (VAL_INDEX 1)
        (obj (vector 0 #(#f))))
  (let ((dv-endptr (lambda () (vector-ref obj FREE_INDEX)))
        (dv-vect   (lambda () (vector-ref obj VAL_INDEX))))
  (letrec
    ((least-2power-more-than (lambda (x acc)
       (if (< x acc) acc (least-2power-more-than x (+ acc acc)))))
     (vector-copy! (lambda (v1 v2 . args)
      (if (null? args)
        (vector-copy! v1 v2 0 0)
        (let ((i1 (car args)) (i2 (cadr args)))
          (cond
            ((<= (vector-length v1) i1) v1)
            ((<= (vector-length v2) i2) v1)
            (else
              (begin
                (vector-set! v1 i1 (vector-ref v2 i2))
                (vector-copy! v1 v2 (+ 1 i1) (+ 1 i2)))))))))

      (dv-append (lambda (val)
        (let ((capacity (vector-length (dv-vect)))
              (index (dv-endptr)))
           (cond
             ((<= capacity index)
               (let ((new-vect (make-vector (least-2power-more-than capacity 1))))
                 (vector-copy! new-vect (dv-vect))
                 (vector-set! obj VAL_INDEX new-vect))))
           (vector-set! (dv-vect) index val)
           (vector-set! obj FREE_INDEX (+ 1 index))
           index)))
      (dv-shrink (lambda ()
        (let ((newvect (make-vector (dv-endptr))))
          (vector-copy! newvect (dv-vect))
          (vector-set! obj VAL_INDEX newvect)))))
    (lambda (msg . args)
      (cond
        ((eq? msg 'append) (dv-append (car args)))
        ((eq? msg 'get)    (dv-vect))
        ((eq? msg 'size)   (dv-endptr))
        ((eq? msg 'at)     (vector-ref (dv-vect) (car args)))
        ((eq? msg 'set)    (vector-set! (dv-vect) (car args) (cadr args)))
        ((eq? msg 'shrink) (dv-shrink))
        (else 'do-not-understand)))))))

(define (make-nfa)
  (let ((dv (make-dynamic-vector)))
    (lambda (msg . args)
      (cond
        ((eq? msg 'get)      (dv 'get))
        ((eq? msg 'size)     (dv 'size))
        ((eq? msg 'new-node) (dv 'append '() ))
        ((eq? msg 'new-edge)
          (let ((from (car args)) (to (cadr args)) (val (caddr args)))
            (dv 'set from (cons (cons to val) (dv 'at from)))))
        (else 'do-not-understand)))))

;; regex parsing
;; Grammar:
;;    ;; atomic expression:
;;    <aexp> = <const>
;;           | .
;;           | ( <expr> )
;;           | [ <set> ]
;;    ;; pipable expression
;;    <rexpr> = <aexpr>*
;;            | <aexpr>?
;;            | <aexpr>+
;;            | <aexpr>
;;    <pexpr> = <rexpr><pexpr>
;;            | <pexpr>
;;    ;; top-level expression
;;    <expr> = <pexpr> | <expr>
;;           | <pexpr>
;;
;;    <set> = ... ; TODO

(define (re-parse-aexpr nfa str)
  (cond
    ((null? str) '()) ;; eof, failed
    ((eq? #\( (car str))
      (let ((e1 (re-parse-expr nfa (cdr str))))
        (if (null? e1) '()
          (let ((str1 (car e1)) (startend1 (cdr e1)))
            (cond
              ((null? str1) '())
              ((eq? (car str1) #\) )
                (cons (cdr str1) startend1))
              (else '()))))))
    ;((eq? #\. (car str)) ; TODO
    ;((eq? #\[ (car str)) ; TODO
    ((memq (car str) '(#\) #\[ #\* #\? #\+ #\|))
      '())
    (else
      (let ((start (nfa 'new-node))
            (end (nfa 'new-node))
            (chr (if (eq? (car str) #\\) (cadr str) (car str))))
      (let ((node-val (char->integer chr)))
        (nfa 'new-edge start end (if (<= #x80 node-val) #x80 node-val))
        (cons (cdr str) (cons start end)))))))

(define (re-parse-rexpr nfa str)
  (let ((a1 (re-parse-aexpr nfa str)))
    (if (null? a1) '()
      (let ((str1 (car a1)) (start1 (cadr a1)) (end1 (cddr a1)))
        (cond
          ((null? str1) a1)  ;; eof
          ((eq? (car str1) #\*)
            (begin
              (nfa 'new-edge end1 start1 EPSILON)
              (nfa 'new-edge start1 end1 EPSILON)
              (cons (cdr str1) (cons start1 end1))))
          ((eq? (car str1) #\?)
            (let ((end (nfa 'new-node)))
              (nfa 'new-edge start1 end EPSILON)
              (nfa 'new-edge end1 end EPSILON)
              (cons (cdr str1) (cons start1 end))))
          ; TODO: ((eq? (car str1) #\+
          (else a1))))))

(define (re-parse-pexpr nfa str)
  ;; returns (str . (start . end))
  (let ((r1 (re-parse-rexpr nfa str)))
    (if (null? r1) '()  ;; parsing failed
      (let ((str1 (car r1)) (start1 (cadr r1)) (end1 (cddr r1)))
        (if (null? str1) r1  ;; eof
            (let ((p2 (re-parse-pexpr nfa str1)))
              (if (null? p2)
                r1   ;; back track
                (let ((str2 (car p2)) (start2 (cadr p2)) (end2 (cddr p2)))
                  (nfa 'new-edge end1 start2 EPSILON)
                  (cons str2 (cons start1 end2))))))))))

(define (re-parse-expr nfa str)
  ;; returns (str . (start . end))
  (let ((p1 (re-parse-pexpr nfa str)))
    (if (null? p1) '()  ;; parsing failed?
      ;; ok
      (let ((str1 (car p1)) (start1 (cadr p1)) (end1 (cddr p1)))
        (cond
          ((null? str1) p1) ;; eof, return pexpr
          ((eq? (car str1) #\|) ;; pipe
            (let ((e2 (re-parse-expr nfa (cdr str1))))
              (if (null? e2) '()
                (let ((str2 (car e2)) (start2 (cadr e2)) (end2 (cddr e2)))
                (let ((start (nfa 'new-node)) (end (nfa 'new-node)))
                  (begin
                    (nfa 'new-edge start start1 EPSILON)
                    (nfa 'new-edge start start2 EPSILON)
                    (nfa 'new-edge end1 end EPSILON)
                    (nfa 'new-edge end2 end EPSILON)
                    (cons str2 (cons start end))))))))
          (else p1))))))

;; regex string => NFA
(define (re-parse str)
  (let ((nfa (make-nfa)))
  (let ((result (re-parse-expr nfa (string->list str))))
    (cons (nfa 'get) result))))


(define (reverse-vect-fold/index vect state fun)
  ;; folds vector in reverse order calling `fun` as
  ;;    (fun <index> <value at index> <accumulator>
  (letrec
     ((fold (lambda (index state)
        (cond
          ((<= 0 index)
            (fold
              (- index 1)
              (fun index (vector-ref vect index) state)))
          (else state)))))
    (fold (- (vector-length vect) 1) state)))

(define (epsilon-closure nfa vertset)
  ;; takes nfa array, index of a vertex
  ;; returns epsilon-closure of the vertex as a list of vertices
  (let ((marks (make-vector (vector-length nfa) #f)))
    (letrec
      ((mark-epsilon-neighbours (lambda (i)
         (cond
           ((vector-ref marks i) #f)
           (else
             (vector-set! marks i #t)
             (for-each
               (lambda (neighb)
                 (let ((j (car neighb)) (val (cdr neighb)))
                   (if (eq? val EPSILON)
                       (mark-epsilon-neighbours j)
                       #f)))
               (vector-ref nfa i)))))))
      (begin
        (for-each
          (lambda (index) (mark-epsilon-neighbours index))
          vertset)
        (reverse-vect-fold/index
          marks '()
          (lambda (ind val acc)
            (if val (cons ind acc) acc)))))))

(define (moveset nfa fromset val)
  ;; takes nfa array, list of vertices `fromset', character code `val'
  ;; returns list of verices accesible from `fromset' with `val'
  (let ((marks (make-vector (vector-length nfa) #f)))
    (letrec
      ((mark-neighbours (lambda (i)
        (for-each
          (lambda (neighb)
            (let ((j (car neighb)) (v (cdr neighb)))
              (if (eq? v val)
                  (vector-set! marks j #t)
                  #f)))
          (vector-ref nfa i)))))
      (begin
        (for-each
          (lambda (i) (mark-neighbours i))
          fromset)
        (reverse-vect-fold/index
          marks '()
          (lambda (ind val acc)
            (if val (cons ind acc) acc)))))))

(define (nfa-alphabet nfa)
  ;; takes nfa array, returns list of character codes
  (let ((charset (make-vector #x81 #f)))
    ;(display ";") (display nfa) (newline)
    (reverse-vect-fold/index nfa #f
      (lambda (ind val _)
        (cond
          ((pair? val)
            (for-each
              (lambda (edge)
                (let ((v (cdr edge)))
                  (cond
                    ((<= v 0) #f)
                    ((<= #x81 v) #f)
                    (else (vector-set! charset v #t)))))
              val))
          (else #f))))
    (reverse-vect-fold/index charset '()
      (lambda (ind val acc)
        (if val (cons ind acc) acc)))))

(define (re-nfa-to-dfa nfa start end)
  (let ((CURRENT-IND 0)
        (state (vector 0))
        (table (make-dynamic-vector)))
  (let ((curr-index (lambda () (vector-ref state CURRENT-IND))))
  (let ((inc-curr-index (lambda ()
          (vector-set! state CURRENT-IND (+ 1 (curr-index))))))
  (let ((search-set (lambda (set)
    ;; returns index of the set if found or #f
    (letrec
      ((vector-index-for (lambda (vct ind pred)
        (cond
          ((< ind 0) #f)
          ((pred (vector-ref vct ind)) ind)
          (else (vector-index-for vct (- ind 1) pred))))))
      (vector-index-for (table 'get) (- (table 'size) 1)
        (lambda (row) (equal? (car row) set)))))))
  (let ((abc (nfa-alphabet nfa)))
  (let ((make-mvsets (lambda (set)
          (map (lambda (chr) (epsilon-closure nfa (moveset nfa set chr))) abc)))
        (set-to-index (lambda (set)
          (if (null? set) -1
              (let ((i (search-set set)))
                (if i i (table 'append (list set))))))))
  (let ((fill-row (lambda ()
      (let ((set (car (vector-ref (table 'get) (curr-index)))))
      (let ((mvsets (map set-to-index (make-mvsets set))))
        (table 'set (curr-index) (cons set mvsets))
        (inc-curr-index))))))
  (begin
    (table 'append (list (epsilon-closure nfa (list start))))
    (let loop ()
      (if (<= (table 'size) (curr-index))
          (begin
            (table 'shrink)
            (table 'get))
          (begin
            ;(newline) (display (curr-index)) (newline)
            ;(display (table 'get)) (newline)
            (fill-row) (loop)))))))))))))

(define (is-final-dfa-row dfa end r)
  (if (memq end (car r)) #t #f))

(define (re-compile regexp)
  (let ((vector-map (if (defined? 'vector-map)
                        vector-map
                        (lambda (f v) (list->vector (map f (vector->list v)))))))
  (let ((r (re-parse regexp))) ; parse result
  (let ((nfa (car r)) (tmp (cdr r)))
  (let ((re-rest (car tmp)) (start (cadr tmp)) (end (cddr tmp)))
    (if (null? re-rest)
      (let ((dfa (re-nfa-to-dfa nfa start end)))
        (list (nfa-alphabet nfa)
              (vector-map (lambda (r) (is-final-dfa-row dfa end r)) dfa)
              (vector-map cdr dfa)))
      (raise 'error:_re-compile_failed_to_parse)))))))

(define (re-match re str)
  (let ((abc (car re))
        (final-states (cadr re))
        (dfa (caddr re)))
  (let loop ((state 0)
             (s (string->list str)))
    (cond
      ((null? s)
        (list (vector-ref final-states state) s))
      (else
        (let ((i (list-index abc (char->integer (car s)))))
          ;(newline) (display (car s)) (newline) (display state) (newline)
          (if i
            (let ((n (list-ref (vector-ref dfa state) i)))
              (if (<= 0 n) (loop n (cdr s)) (list #f s)))
            (list #f s))))))))

;;
;;  Tests
;;

(define re-example1
  '((string . "\"(\\.|[^\"])*\"")
    (decnumber . "[-+]?[0-9]+")))

(define re-test1 "ab*c")
(define re-test2 "(ba*c)*")
(define re-test3 "lol(what|whut)")
(define re-test4 "[01234567]+")

