# gotoForth
An indirect threaded forth implementation that is compatible with ISO C.
It compiles with `-Werror -Wall -Wextra -pedantic`.
Of course a bit of undefined behavior is abused to achive this goal.
This program rely too much on implementation detail to work,
it probably will not work on anyone else's system by default.



## Compile
```console
$ make
$ ./forth
```

## References

- Moving Forth https://www.bradrodriguez.com/papers/index.html
- Forth Standard https://forth-standard.org/standard/alpha
- Starting FORTH https://www.forth.com/starting-forth/
- Thinking Forth https://thinking-forth.sourceforge.net/