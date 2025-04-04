This directory contains executable litmus tests as C source.

## Usage

Compile with your favourite compiler for the desired target architecture as
follows

```bash
clang -O0 -g -o <litmus_test> <litmus_test>.c
```

## Contents

* [reorder](reorder/) - testing the reordering of instructions
