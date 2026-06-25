🐚 BuildShell — A Mini Unix Shell in C++

A lightweight, feature-rich Unix-like shell built from scratch in C++
using system calls, readline, and a lot of curiosity.

BuildShell is a custom command-line shell that supports execution of
system programs, built-in commands, pipelines, redirection, history
management, and tab completion. It is designed to explore how real
shells like bash and zsh work under the hood.

------------------------------------------------------------------------

FEATURES

Core Shell Features - Execute external programs using fork() + execv() -
PATH resolution for commands - Built-in commands support - Robust
command parsing with quotes and escapes

Built-in Commands - cd — change directory - pwd — print working
directory - echo — print arguments - exit — exit shell safely - type —
identify command type - history — command history management

Redirection Supports: > redirect stdout >> append stdout 2> redirect
stderr 2>> append stderr

Example: echo hello > out.txt ls 2> err.txt

Pipelines Chain commands like a real shell: ls | grep cpp | wc -l

Command History - Persistent history using HISTFILE - Load history on
startup - Append history on exit Options: history -r file history -w
file history -a file

Smart Tab Completion - Executables from PATH - Built-in commands -
Longest Common Prefix completion - Double-TAB to list suggestions

Quote Handling - Single quotes - Double quotes with escapes - Backslash
escaping

------------------------------------------------------------------------

TECH STACK

Language: C++

Libraries / APIs: - POSIX system calls (fork, execv, dup2, pipe) - GNU
Readline - Filesystem & directory APIs - STL

------------------------------------------------------------------------

GETTING STARTED

1.  Clone the repo git clone https://github.com/Lambo-IITian/Shell.git
    cd Shell

2.  Compile g++ main.cpp -lreadline -o buildshell

3.  Run ./buildshell

------------------------------------------------------------------------

LEARNING GOALS

This project explores: - How shells interpret commands - Process
creation and management - File descriptor manipulation - Pipes and IPC -
Environment variables and PATH resolution - Terminal interaction using
readline

------------------------------------------------------------------------

EXAMPLE USAGE

$ echo “Hello Shell” $ cd .. $ pwd $ ls | grep .cpp $ history $ type ls

------------------------------------------------------------------------

FUTURE IMPROVEMENTS

-   Job control (fg, bg, jobs)
-   Signal handling
-   Environment variable expansion
-   Globbing
-   Aliases
-   Scripting support

------------------------------------------------------------------------

CONTRIBUTING

Contributions are welcome. Open a PR or issue if you have ideas.

------------------------------------------------------------------------

AUTHOR

Mohit Harjani IIT BHU Systems enthusiast • Builder • Curious engineer

Built to understand the machine, not just use it.
