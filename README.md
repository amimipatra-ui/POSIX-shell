# POSIX shell

```
┌────────────────────────────────────────────────────────┐
│ POSIX SHELL                                            │
│ ────────────────────────────────────────────────────── │
│ a POSIX-style shell written from scratch in C++17 —    │
│ process management, job control, and line editing      │
│ built directly on Unix syscalls, no shell libraries    │
│ used.                                                  │
└────────────────────────────────────────────────────────┘
```

```
┌────────────────────────────────────────────────────────┐
│ WHY                                                    │
│ ────────────────────────────────────────────────────── │
│ Built as a systems-programming project to understand   │
│ what a shell actually does under the hood: process     │
│ creation, signal delivery, terminal ownership, and job │
│ control — the mechanisms every Unix shell relies on    │
│ but few people ever implement themselves.              │
└────────────────────────────────────────────────────────┘
```

```
┌────────────────────────────────────────────────────────┐
│ FEATURES                                               │
│ ────────────────────────────────────────────────────── │
│ - command execution via fork() / execvp()              │
│ - pipelines (ls | grep .cpp | wc -l)                   │
│ - i/o redirection (>, >>, <)                           │
│ - job control: bg execution (&), jobs, fg, bg          │
│ - Ctrl+Z suspend/resume                                │
│ - correct signal handling — Ctrl+C kills the           │
│   foreground job without killing the shell             │
│ - process groups + terminal ownership                  │
│   (setpgid, tcsetpgrp) for job control                 │
│ - readline integration: history, line editing,         │
│   tab completion (builtins + $PATH)                    │
│ - git-aware prompt (shows current branch)              │
│ - quote handling and escaping                          │
│ - variable expansion, exit status tracking             │
│   matching real shell conventions (128+signal)         │
│ - glob expansion via the glob() API                    │
│ - alias / unalias support                              │
│ - command chaining                                     │
└────────────────────────────────────────────────────────┘
```

```
┌────────────────────────────────────────────────────────┐
│ BUILD                                                  │
│ ────────────────────────────────────────────────────── │
│ requires GNU readline                                  │
│ (not the macOS-default libedit)                        │
│                                                        │
│ $ brew install readline   # macOS only                 │
│                                                        │
│ $ clang++ -std=c++17 \                                 │
│     -I/opt/homebrew/opt/readline/include \             │
│     -L/opt/homebrew/opt/readline/lib \                 │
│     -lreadline shell.cpp -o shell                      │
│                                                        │
│ $ ./shell                                              │
│                                                        │
│ # or, using the included Makefile:                     │
│ $ make run                                             │
└────────────────────────────────────────────────────────┘
```

```
┌────────────────────────────────────────────────────────┐
│ USAGE                                                  │
│ ────────────────────────────────────────────────────── │
│ $ sleep 100 &   # run in background                    │
│ $ jobs          # list background/stopped jobs         │
│ $ fg            # bring most recent job to fg          │
│ $ bg            # resume a stopped job in bg           │
└────────────────────────────────────────────────────────┘
```

```
