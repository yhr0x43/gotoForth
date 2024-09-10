# gotoForth
An indirect threaded forth implementation that is compatible with ISO C.
It compiles with `-Werror -Wall -Wextra -pedantic`.
Of course a bit of undefined behavior is abused to achive this goal.
Only x86_64-linux-gnu with gcc version 11.4.0 is tested


## Compile
```console
$ make
$ ./forth
```

## References

- Moving Forth https://www.bradrodriguez.com/papers/index.html
- Forth Standard https://forth-standard.org/standard/alpha
- Starting FORTH https://www.forth.com/starting-forth/