*For use in a Bash environment*

Assuming gcc compiler is installed, to compile the program, use 
the following command:

$ gcc -Wall -Werror -O0 -g3 -std=c11 smallsh.c -o smallsh


You may run the testscript provided to see likely output by placing the testscript in the same directory as the 
C file and executable file, then proceeding to execute the following command(s):


$ chmod +x ./p3testscript    /*(only execute if necessary)*/

$ ./p3testscript 2>&1
