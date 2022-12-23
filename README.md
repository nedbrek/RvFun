# RvFun
Functional model for RISC V

## Building the library
1. Run `make`

## Using the standalone executable
1. Run `make driver.exe`
1. Run `./driver.exe <elf>` (the elf must be statically linked)
1. You can use `-i <count>` to limit the number of instructions executed

## Using the dataflow viewer
1. Run `make dfg.exe`
1. Put the opcodes into a file, one opcode per line (hex numbers, without leading 0x)
1. Run `./dfg.exe -p -f test.code.txt`
1. (stdout gets an annotated assembly view, the dataflow graph is written to dfg.dot)
1. (leave off -p to just get standard out)

## See Also
1. [Tcl bindings](https://github.com/nedbrek/tcl-RvFun)
1. [Python bindings](https://github.com/nedbrek/python-RvFun)

