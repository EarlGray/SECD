(define re-example1
  '((string . "\"(\\.|[^\"])*\"")
    (decnumber . "[-+]?[0-9]+")))

(define END-OF-STREAM #x00)
(define EPSILON       #xFE)
(define STUCK         #xFF)

(define FINAL_INDEX   0)

(define re-ex1  ;; "ab*c"
  (let ((re (vector (make-vector #x81 STUCK)
                    (make-vector #x81 STUCK)
                    (make-vector #x81 STUCK))))
    (begin
      (vector-set! (vector-ref re 2) FINAL_INDEX #t)

      (vector-set! (vector-ref re 0) (char->integer #\a) 1)
      (vector-set! (vector-ref re 1) (char->integer #\b) 1)
      (vector-set! (vector-ref re 1) (char->integer #\c) 2)
      re)))

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
