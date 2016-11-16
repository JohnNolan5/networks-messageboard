John Nolan
jnolan5
John Riordan
jriord2

# networks-messageboard

John Nolan
John Riordan

Program 4
A message board service, using both udp and tcp.

Includes the server and client programs in seperate directories.

The program directory includes the following files:

README.md
client/myfrm.c
client/myfrm.h
client/Makefile
server/myfrmd.c
server/Makefile

Perform 'make' in both directories to compile the two programs; 'make clean' to remove the execuable.

Start the server program first by calling the executable with the port number and the administrative password:

./myfrmd 41044 password

Then start the client program (on a different terminal) by calling the executable with the server name and port number:

./myfrm localhost 41044 

Then enter any username and password into the client program, and follow the prompts from there!
