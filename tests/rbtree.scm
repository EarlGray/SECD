(define BLACK 0)
(define RED   1)

(define-macro (childs node) (list 'car node))
(define-macro (rbdata node) (list 'cdr node))

(define-macro (leftchild node)  (list 'car (list 'childs node)))
(define-macro (rightchild node) (list 'cdr (list 'childs node)))
(define-macro (rbval  node)     (list 'car (list 'rbdata node)))
(define-macro (rbcolor node)    (list 'cdr (list 'rbdata node)))

(define-macro (make-childs left right) (list 'cons left right))
(define-macro (make-rbinfo data color) (list 'cons data color))
(define-macro (make-node left right data color)
    (list 'cons (list 'make-childs left right) (list 'make-rbinfo data color)))

(define-macro (make-node-with-rbinf l r i)
    (list 'cons (list 'cons l r) i))

(define (make-leaf val) (make-node '() '() val RED))

(define (tree-set-left node left)
    (make-node-with-rbinf left (rightchild node) (rbdata node)))
(define (tree-set-right node right)
    (make-node-with-rbinf (leftchild node) right (rbdata node)))

(define (bintree-insert tree val cmp)
  (if (null? tree) (make-leaf val)
      (let ((compared (cmp data (rbval tree))))
        (cond
          ((eq? compared 0) tree) ;; already exists
          ((<= compared 0)
            (let ((l (leftchild tree)))
              (if (null? l)
                  (tree-set-left tree (make-leaf val))
                  (tree-set-left tree (bintree-insert l val cmp)))))
          ((<= 0 compared)
            (let ((r (rightchild tree)))
               (if (null? r)
                  (tree-set-right tree (make-leaf val))
                  (tree-set-right tree (bintree-insert r val cmp)))))))))


