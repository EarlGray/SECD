(load "tests/hashtable.scm")
(load "tests/secdtool.scm")
(display (hashtable-keys (free-variables (closure-func free-variables))))
