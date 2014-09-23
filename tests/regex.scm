;;
;; regular expressions
;;

(define END-OF-STREAM #x00)
(define EPSILON       #xFE)
(define STUCK         #xFF)

(define FINAL_INDEX   0)

(define (vector-copy! v1 v2 . args)
  (if (null? args)
    (vector-copy! v1 v2 0 0)
    (let ((i1 (car args)) (i2 (cadr args)))
      (cond
        ((<= (vector-length v1) i1) v1)
        ((<= (vector-length v2) i2) v1)
        (else
          (begin
            (vector-set! v1 i1 (vector-ref v2 i2))
            (vector-copy! v1 v2 (+ 1 i1) (+ 1 i2))))))))

(define (make-nfa)
  (let ((FREE_INDEX 0)
        (NODESV     1)
        (obj (vector 0 #(#f) )))
    (let ((my-endptr (lambda () (vector-ref obj FREE_INDEX)))
          (my-nodes  (lambda () (vector-ref obj NODESV)))
          (my-capacity (lambda () (vector-length (vector-ref obj NODESV)))))
      (letrec
        ((add-node (lambda ()
          (let ((index (my-endptr)))
            (cond
              ((<= (my-capacity) index)
                ;; expand arr
                (let ((newnodesv (make-vector  (* 2 (my-capacity)))))
                  (vector-copy! newnodesv (my-nodes))
                  (vector-set! obj NODESV newnodesv))))
            ; init with an empty alist:
            (vector-set! (vector-ref obj NODESV) index '())
            (vector-set! obj FREE_INDEX (+ 1 index))
            index)))
         (add-edge (lambda (from to val)
           (let ((edges (vector-ref (my-nodes) from)))
             (vector-set! (my-nodes) from (cons (cons to val) edges))))))
       (lambda (msg . args)
         (cond
           ((eq? msg 'get)      (my-nodes))
           ((eq? msg 'size)     (my-endptr))
           ((eq? msg 'new-node) (add-node))
           ((eq? msg 'new-edge) (add-edge (car args) (cadr args) (caddr args)))
           (else 'do-not-understand)))))))

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
    ;((eq? #\. (car str))
    ;((eq? #\[ (car str))
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

;(define (re-nfa-to-dfa nfa)
;  )

; TODO: regex-parsing => NFA => DFA
;(define (re-compile regexp)

(define (final-state? re state)
  (not (eq? STUCK (vector-ref (vector-ref re state) FINAL_INDEX))))
(define (next-state-for re state c)
  (vector-ref (vector-ref re state) c))

(define (re-match re str)
  (let loop ((state 0)
             (s (map char->integer (string->list str))))
    (if (null? s)
      (list (final-state? re state) s)
      (let ((c (car s)))
        (let ((next-state (next-state-for re state (if (<= #x80 c) #x80 c))))
          (cond
            ((eq? next-state STUCK)
              (list (final-state? re state) s))
            (else (loop next-state (cdr s)))))))))


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

(define re-dfa-ex1  ;; "ab*c"
  (let ((re (vector (make-vector #x81 STUCK)
                    (make-vector #x81 STUCK)
                    (make-vector #x81 STUCK))))
    (begin
      (vector-set! (vector-ref re 2) FINAL_INDEX #t)

      (vector-set! (vector-ref re 0) (char->integer #\a) 1)
      (vector-set! (vector-ref re 1) (char->integer #\b) 1)
      (vector-set! (vector-ref re 1) (char->integer #\c) 2)
      re)))

