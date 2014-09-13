(define BLACK 0)
(define RED   1)

(define-macro (childs node) (list 'car node))
(define-macro (rbdata node) (list 'cdr node))

(define-macro (lchild node)  (list 'car (list 'childs node)))
(define-macro (rchild node) (list 'cdr (list 'childs node)))
(define-macro (rbval  node)     (list 'car (list 'rbdata node)))
(define-macro (rbcolor node)    (list 'cdr (list 'rbdata node)))

(define-macro (make-childs left right) (list 'cons left right))
(define-macro (make-rbinfo data color) (list 'cons data color))
(define-macro (make-node left right data color)
    (list 'cons (list 'make-childs left right) (list 'make-rbinfo data color)))

(define-macro (make-node-with-rbinf l r i)
    (list 'cons (list 'cons l r) i))

(define (has-rchild t)
    (not (eq? (rchild t) '())))
(define (has-lchild t)
    (not (eq? (lchild t) '())))

(define (make-leaf val) (make-node '() '() val RED))

(define (tree-set-left node left)
    (make-node-with-rbinf left (rchild node) (rbdata node)))
(define (tree-set-right node right)
    (make-node-with-rbinf (lchild node) right (rbdata node)))

(define (bintree-search tree val cmp)
  (if (null? tree) '()
      (let ((compared (cmp val (rbval tree))))
        (cond
          ((eq? compared 0) (list (rbval tree)))
          ((<= compared 0) (bintree-search (lchild tree) val cmp))
          ((<= 0 compared) (bintree-search (rchild tree) val cmp))))))

(define (tree-insert t val cmp rebalance)
  (if (null? t) (make-leaf val)
    (let ((compared (cmp val (rbval t))))
      (cond
        ((eq? compared 0) t)
        ((<= compared 0)
          (let ((l (lchild t)))
            (if (null? l)
              (rebalance (tree-set-left t (make-leaf val)))
              (rebalance (tree-set-left t (tree-insert l val cmp rebalance))))))
        ((<= 0 compared)
          (let ((r (rchild t)))
            (if (null? r)
              (rebalance (tree-set-right t (make-leaf val)))
              (rebalance (tree-set-right t (tree-insert r val cmp rebalance))))))))))

(define (bintree-insert tree val cmp)
   (tree-insert tree val cmp (lambda (x) x)))

(define (rbtree-insert t val cmp)
  (let ((t1 (tree-insert t val cmp rb-rebalance)))
    (make-node (lchild t1) (rchild t1) (rbval t1) BLACK)))

(define (andf . thunks)
  (cond
    ((null? thunks) #t)
    (((car thunks)) (apply andf (cdr thunks)))
    (else #f)))

(define (rb-rebalance t)
  (cond
    ((not (eq? BLACK (rbcolor t))) t)
    ((andf (lambda () (has-rchild t))
           (lambda () (eq? RED (rbcolor (rchild t))))
           (lambda () (has-rchild (rchild t)))
           (lambda () (eq? RED (rbcolor (rchild (rchild t))))))
       (let ((r (rchild t)) (rr (rchild (rchild t))))
         (make-node
           (make-node (lchild t) (lchild r) (rbval t) BLACK)
           (make-node (lchild rr) (rchild rr) (rbval rr) BLACK)
           (rbval r) RED)))
    ((andf (lambda () (has-rchild t))
           (lambda () (eq? RED (rbcolor (rchild t))))
           (lambda () (has-lchild (rchild t)))
           (lambda () (eq? RED (rbcolor (lchild (rchild t))))))
       (let ((r (rchild t)) (lr (lchild (rchild t))))
         (make-node
           (make-node (lchild t) (lchild lr) (rbval t) BLACK)
           (make-node (rchild lr) (rchild r) (rbval r) BLACK)
           (rbval lr) RED)))
    ((andf (lambda () (has-lchild t))
           (lambda () (eq? RED (rbcolor (lchild t))))
           (lambda () (has-rchild (lchild t)))
           (lambda () (eq? RED (rbcolor (rchild (lchild t))))))
       (let ((l (lchild t)) (rl (rchild (lchild t))))
         (make-node
           (make-node (lchild l) (lchild rl) (rbval l) BLACK)
           (make-node (rchild rl) (rchild t) (rbval t) BLACK)
           (rbval rl) RED)))
    ((andf (lambda () (has-lchild t))
           (lambda () (eq? RED (rbcolor (lchild t))))
           (lambda () (has-lchild (lchild t)))
           (lambda () (eq? RED (rbcolor (lchild (lchild t))))))
       (let ((l (lchild t)) (ll (lchild (lchild t))))
         (make-node
           (make-node (lchild ll) (rchild ll) (rbval ll) BLACK)
           (make-node (rchild l) (rchild t) (rbval l) BLACK)
           (rbval l) RED)))
    (else t)))
