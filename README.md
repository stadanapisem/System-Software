# Assembler and Emulator
            A project consisting of an assembler and an emulator for a simplified architecture.
Assembler is implemented with two passes. First pass tokenizes whole input file, than parses the tokens for directives, sections, data definitions and instructions. During the first pass it populates the symbol table and determines the size of each section. During the second pass it populates the section data with correct data initialization and codes for instructions. Furthermore, in this pass relocation table gets filled.
Emulator is made to run on 32bit machines, and it is able to reference 4GB of its virtual address space. Thus, this project uses page organization with a swap file in memory. Firstly, emulator loads object file created by assembler and checks whether all the sections can be stored on their defined addresses. Secondly, if all checks pass it loads interrupt routines and interrupt vector table. Finally, it loads the user programme into memory and starts executing it.

## Architecture
 - A 32bit RISC processor, simplified for education purpuses
 - Load/Store RISC architecture
 - 32bit virtuall address space, 1B adressible unit, little-endian
 - 3 Address instructions for integer arithmetic (there is no support for real numbers)
 
    ### Registers
     - 16 32bit all purpose registers from R0 to R15
     - 32bit Programme counter register
     - 32bit Stack pointer register, pointing to the word at the top of the stack. Stack rises with adresses
 
    ### Address Modes
     - Immediate addressing: ```#constant_expression```
     - Register direct: ```Ri```
     - Memory direct: ```address```; where ```address``` is a constant expression
     - Register indirect: ```[Ri]```
     - Register indirect with offset: ```[Ri + offset]``` where ```offset``` is a constant expression
     - Registers available for use in all address modes: R0 to R15; PC, SP

    #### Address Modes usage
     - In control flow instructions: all address modes
     - In load/store instructions: all (except immediate for store)
     - In stack and arithmetic/logic instructions: only register direct

    #### Instruction format
     - In register direct and indirect:
        |31|23|20|15|10|5|2|
        |---|---|---|---|---|---|---|
        |Op code|Addr mode|Reg0|Reg1|Reg2|Type|Unused|

     - In immediate, memory direct, register indirect with offset:
        -- First double word (first 32bits):
        
        |31|23|20|15|10|5|2|
        |---|---|---|---|---|---|---|
        |Op code|Addr mode|Reg0|Reg1|Reg2|Type|Unused|
        
        -- Second double word (second 32bits):
        |31|
        |---|
        |Constant / Address / Displacement|asd|
    
     - Address mode codes:
        |immediate|register direct|mem direct|register indirect|reg indirect w/ offset|
        |---|---|---|---|---|
        |0b100|0b000|0b110|0b010|0b111
    
     - Register codes:
        |R0..R15|SP|PC
        |---|---|---|
        |0x0..0xf|0x10|0x11|

    ### Assembler 
    Assembly code consists of expressions, instructions and directives. Instruction forrmat is as follows: ```label: operation oper1, oper2, oper3 ; comment```. A label may be used in constant expressions and it is calculated as a address of the instruction that it precedes.
    
    ### Defining data
    - Definition format:
            ```[label:] def data-specifier [, ...] [; comment]```
        - Data specifiers:
            - ```DB```: Defines one byte
            - ```DW```: Defines a word (2 bytes)
            - ```DD```: Defines a double word (4 bytes)
        Specification of initial values:
        ```constant_expression [DUP constant_expression | ?]```
    - Constant expression: literals, integer arithmetic operators (+, -, *, /), subterms in brackets
    - Literal: signed integer in decimal, binary or hex form, like in ```C```
    - ? for undefined initial values (used in ```.bss``` section)
    - ```DUP```: first term defines how many values of the second term to put into memory
    
    ### Directives
    - Defining a symbolic constant which may be used in constant expressions:
        ```symbol DEF constant-expression [; comment]```
    - Implicitly defined symbol ```$``` has the value of the instruction it is used in
    - Defining absolute address of next instruction:
        ```ORG constant-expression [; comment]```
    - Defining programm segments:
        - ```.text[.number]``` - Section with programm code
        - ```.data[.number]``` - Section with initialized data
        - ```.rodata[.number]``` - Section with read-only data
        - ```.bss[.number]``` - Section with uninitialized data

    ## Emulator
    - Interrupt vector table begins from address 0 and has 32 enties
    - Executing instruction ```INT 0``` ends programm
    - Entry ```0``` has the initial value of the stack pointer register
    - Entry ```3``` has the address of the routine executed in case of error
    - Entry ```4``` has the address of the routine executed periodically every ```0.1s```
    - Entry ```5``` has the address of the routine executed every time a key is pressed
    - Interrupt nesting is not allowed
    - There are two memory mapped register after the interrupt vector table. Both are one byte long, but occupy 4 bytes in memory for easier addressing (everything is divisible by 4). The first register contains value to be printed to the standard output. The second register contains value read from the standard output.
    
## Prerequisites
    cMake 3.7 or newer
    g++5 or newer
    
## Compiling
    Project is compiler using cMake and the provided cMake script

Project was done as a compulsary part of the Systems Software course at University of Belgrade - School of Electrical Engineering.
