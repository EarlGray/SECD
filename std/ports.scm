(define (current-input-port) *stdin*)
(define (current-output-port) *stdout*)
(define (current-error-port) *stdout*)  ;; TODO

(define (input-port? obj)
  (cdr (assq 'in (secd-port-info obj))))
(define (output-port? obj)
  (cdr (assq 'out (secd-port-info obj))))

(define (textual-port? obj)
  (cdr (assq 'txt (secd-port-info obj))))
(define (binary-port? obj)
  (not (textual-port? obj)))

(define (input-port-open? port)
  (input-port? port))
(define (output-port-open? port)
  (output-port? port))

(define (with-input-from-file filename thunk)
  (let ((*stdin* (open-input-file filename)))
    (thunk)))

(define (with-input-from-string str thunk)
  (let ((*stdin* (open-input-string str)))
    (thunk)))

(define (eof-object)  (string->symbol "#<eof>"))
