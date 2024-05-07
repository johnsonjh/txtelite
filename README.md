# Text Elite

This is an updated and bugfixed version of **Text Elite** 1.5.

It should build and run on any POSIX.1-2001 system with a
C89 compiler, and has been tested on Linux, AIX, Haiku,
\*BSD, DJGPP, macOS, Cygwin, and Windows.

**Text Elite** is a C implementation of the classic Elite
trading system with a text adventure style shell, originally
coded to formalise and archive the definition of the Classic
Elite Universe.

**Text Elite** is trading only. There is no risk of misjump,
police, or pirate attack.

Note that **Text Elite**'s Galactic Hyperspace jumps to the
system in the target Galaxy having the same position in the
Galactic Generation sequence as the planet jumped from.
This is a deviation from Classic 6502 Elite, which always
jumped to a common "central" system (nearest (#0x60,#x60)).

You can send scripts to `txtelite` via redirection or pipes
using a command such as `./txtelite < ./script.txt`. Two such
scripts are included.

If you want input line-editing and command history, use
[`rlwrap`](https://github.com/hanslub42/rlwrap), *i.e.*
`rlwrap ./txtelite`.
