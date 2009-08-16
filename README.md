# sandbox

Runs program in sandbox. Program has to be compiled as shared library (`.so`; add flag `-shared` to `gcc`) and has to have `main` symbol visible. (no `gcc` flag `-fvisibility=hidden`).

It is not another jailing program, because the program binary does not have to be in jail itself.

## Compile

    $ make

## Install

    # cp ./sandbox /usr/local/bin
