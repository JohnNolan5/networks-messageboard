// myfrmd.c
// John Nolan
// And Riordan
// jnolan5
// jriorda2

// usage: myfrmd port

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h> // for the unix commands
#include <dirent.h> // for directory information
#include <sys/stat.h> // for directory status
#include <mhash.h>

#define MAX_PENDING 5
#define MAX_LINE 256

void handle_input(char* msg, int s, const char *username);

void create_board(int, const char *username);
void leave_message(int, const char *);
bool check_board(const char*);
bool check_board_file(const char* board_name, const char* file_name);
void delete_board_name(const char*);
void delete_message(int);
void edit_message(int);
void list_boards(int);
void read_board(int);
void append_file(int);
void download_file(int);
void destroy_board(int);

int receive_instruction(int,char**);
void send_result(int, short);
int receive_file(int,FILE**); 

int s_d, s, new_s, addr_len;
struct sockaddr_in s_in;
char username[MAX_LINE];

int main( int argc, char* argv[] ){
	char buf[MAX_LINE], password[MAX_LINE], msg[1];
	FILE* fp;
	int len, opt;
	uint16_t port;
	int response; // response to send back to 

	addr_len = sizeof(struct sockaddr_in);

	// validate and put command line in variables
	if( argc == 2 ){
		port = atoi(argv[1]);
	} else {
		fprintf( stderr, "usage: myfrmd port\n" );
		exit( 1 );
	}

	// build address data structure
	bzero( (char*)&s_in, sizeof( s_in ) );
	s_in.sin_family = AF_INET;
	s_in.sin_addr.s_addr = INADDR_ANY;// use default IP address
	s_in.sin_port = htons( port );

	// create the server-side socket
	if( ( s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ){
		fprintf( stderr, "myfrmd: socket creation error\n" );
		exit( 1 );
	}
	
	if( ( s_d = socket( PF_INET, SOCK_DGRAM, 0 )) < 0) {
		fprintf( stderr, "myfrmd: udp socket creation error\n" );
		exit( 1 );
	}

	// set socket options
	opt = 1;
	if( ( setsockopt( s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(int) ) ) < 0 ){
		fprintf( stderr, "myfrmd: error setting socket options\n" );
		exit( 1 );
	}

	// bind the created socket to the address
	if( bind( s, (struct sockaddr*)&s_in, sizeof( s_in ) ) < 0 ){
		fprintf( stderr, "myfrmd: socket binding error\n" );
		exit( 1 );
	}

	// bind the udp socket to the address
	if( bind( s_d, (struct sockaddr*)&s_in, sizeof( s_in ) ) < 0 ){
		fprintf( stderr, "myfrmd: udp socket binding error\n" );
		exit( 1 );
	}

	// open the passive socket by listening
	if( ( listen( s, MAX_PENDING ) ) < 0 ){
		fprintf( stderr, "myfrmd: socket listening error\n" );
		exit( 1 );
	}

	// wait for connection, then receive and print text
	while( 1 ){
		if( ( new_s = accept( s, (struct sockaddr*)&s_in, &len ) ) < 0 ){
			fprintf( stderr, "myfrmd: accept error, %i\n", new_s );
			exit( 1 );
		}

		send_result( new_s, 1 );

		addr_len = sizeof( struct sockaddr );
		if( recvfrom( s_d, username, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
			fprintf( stderr, "myfrmd: error receiving username\n" );
			exit( 1 );
		}

		send_result(new_s, 2); // request password 
		
		if (read(new_s, password, MAX_LINE) == -1) {
			fprintf( stderr, "myfrmd: error receiving password \n");
			exit( 1 );
		}
	
		fp = fopen("users.txt", "ra+");
		if (fp == NULL) {
			fp = fopen("users.txt", "w+");
		}
		if (fp == NULL) {
			fprintf( stderr, "myfrmd: could not open users file\n");
			send_result(s, -1); // tell client?
			return;
		}
		
		char* user_line;
		char* pass_test;
		char* user_test;
		size_t len = 0;
		bool user_exists = false;
		bool signed_in = false;
		while (getline(&user_line, &len, fp) != -1) {
			if (strlen(user_line) <= 0) continue;
			user_test = strtok(user_line, " \n");
			if (strcmp(user_test, username) == 0) {
				// username in line and s
				user_exists = true;	
				pass_test = strtok(NULL, " \n");
				if ((strcmp(pass_test, password) == 0)) {
					send_result(new_s, 1); // Signed in! proceed...
					signed_in = true;
				}
			}
			memset(user_line,0,strlen(user_line));
			memset(user_test,0,strlen(user_test));
			memset(pass_test,0,strlen(pass_test));
		}
		if (!user_exists) {
			// append username and password to file
			fprintf(fp, "%s %s\n", username, password);
			fclose(fp);
			signed_in = true;
			send_result(new_s, 1);
		} else if (!signed_in) {
			// not able to sign in
			send_result(new_s, -1); // either password wrong or need a new user name
			fclose(fp);
		}
		while( signed_in ){
			if( read( new_s, buf, 3 ) == -1 ){
				fprintf( stderr, "myfrmd: receive error\n" );
			}
			if( !strncmp( buf, "XIT", 3)) {
				break;
			}
			handle_input(buf, new_s, username);
		}

		close( new_s );
		memset(username,0,strlen(username));
		memset(password,0,strlen(password));
	}

	return 0;
}

void handle_input(char* msg, int s, const char* username) {
	// check for all special 3 char messages
	if( !strncmp("CRT", msg, 3) )
		create_board( s , username);
	else if( !strncmp("MSG", msg, 3) )
		leave_message( s, username );
	else if( !strncmp("DLT", msg, 3) )
		delete_message( s );
	else if( !strncmp("EDT", msg, 3) )
		edit_message( s );
	else if( !strncmp("LIS", msg, 3) )
		list_boards(s);
	else if( !strncmp("RDB", msg, 3) )
		read_board(s);
	else if( !strncmp("APN", msg, 3) )
		append_file( s );
	else if( !strncmp("DWN", msg, 3) )
		download_file( s );
	else if( !strncmp("DST", msg, 3) )
		destroy_board( s );
}

void create_board(int s, const char* username) {

	char board_name[MAX_LINE];	
	char *board_test;
	char *board_line;
	size_t len;
	FILE *fp;

	if (read(s, board_name, MAX_LINE) == -1) {
		fprintf( stderr, "myfrmd: error receiving name of new board\n");
		exit( 1 );
	}

	if (check_board(board_name)) {
		send_result(s, -2); // already exists
		return;
	}
	
	fp = fopen("boards.txt", "a+");
	if (fp == NULL) {
		fp = fopen("boards.txt", "w+");
		// no boards existing, tell it its new
		if (fp == NULL) {
			fprintf( stderr, "myfrmd: could not open boards file\n");
			send_result(s, -1); // tell client?
			return;
		}
	}
	
	fprintf(fp, "%s\n", board_name); // appends to file or writes at beginning.
	fclose(fp);

	fp = fopen(board_name, "w+");
	if (fp == NULL) {
		fprintf( stderr, "myfrmd: could not open new board\n");
		send_result(s, -1); // tell client?
		return;
	}

	fprintf(fp, "%s\n\n", username);	
	fclose(fp);

	send_result(s, 1);
	return;
	
}

void leave_message( int s , const char* username){
	char *fileName;
	char *message;
	FILE* fp;
	bool board_exists = false;

	receive_instruction(s, &fileName);
	receive_instruction(s, &message);	
	
	board_exists = check_board(fileName);

	if (board_exists) {
		fp = fopen(fileName, "a+");
		if (fp == NULL) {
			fprintf(stderr, "myfrmd: Trouble opening the board\n");
			send_result(s, -2);
			free(fileName);
			free(message);
			return;
		}
		fprintf(fp, "%s\n%s\n\n", username, message);
		fclose(fp);
		send_result(s, 1);

	} else {
		send_result(s, -1); // file does not exist
	}
	free(fileName);
	free(message);

}

bool check_board(const char* board_name) {

	return check_board_file(board_name, NULL);
}

bool check_board_file(const char* board_name, const char* file_name) {	
	char *board_line = NULL;
	char *board_test = NULL;
	FILE *fp;
	size_t len = 0;

	fp = fopen("boards.txt", "r");
	
	if (fp == NULL) {
		return false;
	}
	
	while (getline(&board_line, &len, fp) != -1) { 
		
		if (strcmp(board_line, "\n") == 0) continue;

		board_test = strtok(board_line, " \n");


		if (strcmp(board_test, board_name) == 0) {
		// board_name found
			if (file_name == NULL) {
				fclose(fp);
				return true;
			} else {
				board_test = strtok(NULL, " \n");
				while (board_test != NULL) {
					if (strcmp(board_test, file_name) == 0) {
						fclose(fp);
						return true; // found the board
					}
					board_test = strtok(NULL, " \n");
				}
				return false;
			}
		}
		memset(board_line, 0, strlen(board_line));
		board_line = NULL;
		memset(board_test, 0, strlen(board_test));
		board_test = NULL;

	}
	fclose(fp);
	return false;
}

void add_file_name(const char* board_name, const char* file_name) {
	char *board_line = NULL;
	char board_full[MAX_LINE];
	char *board_test = NULL;
	FILE* fpOld;
	FILE* fpNew;
	FILE* fp;
	size_t len = 0;

	fpOld = fopen("boards.txt", "r");
	fpNew = fopen( "tmpboards.txt", "w");
	
	if( !fpOld || !fpNew ){
		return;
	}
	
	while (getline(&board_line, &len, fpOld) != -1) { 	
		if (strcmp(board_line, "\n") == 0){
			fprintf( fpNew, "\n" );
			continue;
		}
		strncpy(board_full, board_line, strlen(board_line)-1);
	

		board_test = strtok(board_line, " \n");

		if (strlen(board_line) <= 0)
			continue;

		if (strcmp(board_test, board_name) == 0) {
			strcat(board_full, " ");
			strcat(board_full, file_name);		
		}

		fprintf( fpNew, "%s\n", board_full); 

		memset(board_line, 0, strlen(board_line));
		board_line = NULL;
		memset(board_test, 0, strlen(board_test));
		board_test = NULL;
		memset(board_full, 0, strlen(board_full));
	}

	fclose(fpOld);
	fclose(fpNew);

	remove("boards.txt");
	rename("tmpboards.txt", "boards.txt");
	
	fp = fopen(board_name, "a");
	if (fp == NULL) {
		fprintf(stderr, "myfrm: Could not open board to append the filename\n");
		return;
	}

	fprintf( fp, "%s\n%s\n\n", username, file_name); // add file to actual board

	fclose(fp);

}

void delete_board_name(const char* board_name) {	
	char *board_line = NULL;
	char *board_test = NULL;
	FILE* fpOld;
	FILE* fpNew;
	size_t len = 0;

	fpOld = fopen("boards.txt", "r");
	fpNew = fopen( "tmpboards.txt", "w");
	
	if( !fpOld || !fpNew ){
		return;
	}
	
	while (getline(&board_line, &len, fpOld) != -1) { 	
		if (strcmp(board_line, "\n") == 0){
			fprintf( fpNew, "\n" );
			continue;
		}

		board_test = strtok(board_line, " \n");

		if (strlen(board_line) <= 0)
			continue;

		if (strcmp(board_test, board_name) == 0) {
			continue;
		}
		fprintf( fpNew, "%s\n", board_line ); 

		memset(board_line, 0, strlen(board_line));
		board_line = NULL;
		memset(board_test, 0, strlen(board_test));
		board_test = NULL;
	}

	fclose(fpOld);
	fclose(fpNew);

	remove("boards.txt");
	rename("tmpboards.txt", "boards.txt");
}

void delete_message( int s ){
	char boardName[MAX_LINE];
	char *board_line = NULL;
	FILE* fpOld;
	FILE* fpNew;
	size_t len = 0;
	char msg[1];
	bool reading;
	int count = 0;

	// get the board name
	addr_len = sizeof(struct sockaddr);
	if( recvfrom( s_d, boardName, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrmd: error receiving board name\n" );
		exit( 1 );
	}

	// ensure board exists
	if( check_board( boardName ) ){
		// send message to client to verify board exists
		msg[0] = 'y';
		if( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
			fprintf( stderr, "myfrmd: error sending validation board exists\n" );
			exit( 1 );
		}

		// open board and new file to write to
		fpOld = fopen(boardName, "r");
		fpNew = fopen( "tmp.txt", "w");
		if( !fpOld || !fpNew ){
			return;
		}

		// read through board
		reading = false;
		while (getline(&board_line, &len, fpOld) != -1) {
			// we matched username above
			if( reading ){
				// handle trailing newlines and board owner at the top
				if (!strcmp(board_line, "\n")){
					if( count == 1 ){
						count--;
						reading = false;
					} else {
						fprintf( fpNew, "%s\n", username );
						fprintf( fpNew, "\n" );
						reading = false;
					}
					continue;
				}

				// send line over to client and wait for delete response
				if( sendto( s_d, board_line, strlen(board_line)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
					fprintf( stderr, "myfrmd: error sending possible deletion\n" );
					exit( 1 );
				}
				addr_len = sizeof(struct sockaddr);
				if( recvfrom( s_d, msg, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
					fprintf( stderr, "myfrmd: error receiving delete confirmation\n" );
					exit( 1 );
				}

				// either get ready for trailing newlines or copy line back to new file
				if( msg[0] == 'y' ){
					count = 1;
				} else {
					fprintf( fpNew, "%s\n%s", username, board_line );
					reading = false;
				}
				continue;
			}

			// handle corner cases
			if (strcmp(board_line, "\n") == 0){
				fprintf( fpNew, "\n" );
				continue;
			}
			if (strlen(board_line) <= 0)
				continue;

			// try to match username
			if(!strcmp(username, board_line) == 0 ){
				reading = true;
				continue;
			}

			// otherwise, copy line to new file and get ready for next line
			fprintf( fpNew, "%s\n", board_line ); 
			memset(board_line, 0, strlen(board_line));
			board_line = NULL;
		}

		// done reading- tell client
		board_line = "$";
		if( sendto( s_d, board_line, strlen(board_line)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
			fprintf( stderr, "myfrmd: error sending end of deletion asking\n" );
			exit( 1 );
		}

		// replace board with new file
		fclose(fpOld);
		fclose(fpNew);
		remove(boardName);
		rename("tmp.txt", boardName);
	} else {
		// board didn't exist- tell client
		msg[0] = 'n';
		if( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
			fprintf( stderr, "myfrmd: error sending validation board doesn't exist" );
			exit( 1 );
		}
	}
}

void edit_message( int s ){
	char boardName[MAX_LINE];
	char newLine[MAX_LINE];
	char *board_line = NULL;
	FILE* fpOld;
	FILE* fpNew;
	size_t len = 0;
	char msg[1];
	bool reading;

	// get the board name
	addr_len = sizeof(struct sockaddr);
	if( recvfrom( s_d, boardName, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrmd: error receiving board name\n" );
		exit( 1 );
	}

	// ensure board exists
	if( check_board( boardName ) ){
		// send message to client to verify board exists
		msg[0] = 'y';
		if( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
			fprintf( stderr, "myfrmd: error sending validation board exists\n" );
			exit( 1 );
		}

		// open board and new file to write to
		fpOld = fopen(boardName, "r");
		fpNew = fopen( "tmp.txt", "w");
		if( !fpOld || !fpNew ){
			return;
		}

		// read through board
		reading = false;
		while (getline(&board_line, &len, fpOld) != -1) {
			// we matched username above
			if( reading ){
				// handle trailing newlines and board owner at the top
				if (!strcmp(board_line, "\n")){
					fprintf( fpNew, "%s\n", username );
					fprintf( fpNew, "\n" );
					reading = false;
					continue;
				}

				// send line over to client and wait for delete response
				if( sendto( s_d, board_line, strlen(board_line)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
					fprintf( stderr, "myfrmd: error sending possible deletion\n" );
					exit( 1 );
				}
				addr_len = sizeof(struct sockaddr);
				if( recvfrom( s_d, msg, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
					fprintf( stderr, "myfrmd: error receiving delete confirmation\n" );
					exit( 1 );
				}

				// either get edited line or copy line back to new file
				if( msg[0] == 'y' ){
					addr_len = sizeof(struct sockaddr);
					if( recvfrom( s_d, newLine, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
						fprintf( stderr, "myfrmd: error receiving delete confirmation\n" );
						exit( 1 );
					}
					fprintf( fpNew, "%s\n%s", username, newLine );
				} else {
					fprintf( fpNew, "%s\n%s", username, board_line );
				}
				reading = false;
				continue;
			}

			// handle corner cases
			if (strcmp(board_line, "\n") == 0){
				fprintf( fpNew, "\n" );
				continue;
			}
			if (strlen(board_line) <= 0)
				continue;

			// try to match username
			if(!strcmp(username, board_line) == 0 ){
				reading = true;
				continue;
			}

			// otherwise, copy line to new file and get ready for next line
			fprintf( fpNew, "%s\n", board_line ); 
			memset(board_line, 0, strlen(board_line));
			board_line = NULL;
		}

		// done reading- tell client
		board_line = "$";
		if( sendto( s_d, board_line, strlen(board_line)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
			fprintf( stderr, "myfrmd: error sending end of deletion asking\n" );
			exit( 1 );
		}

		// replace board with new file
		fclose(fpOld);
		fclose(fpNew);
		remove(boardName);
		rename("tmp.txt", boardName);
	} else {
		// board didn't exist- tell client
		msg[0] = 'n';
		if( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
			fprintf( stderr, "myfrmd: error sending validation board doesn't exist" );
			exit( 1 );
		}
	}
}

void list_boards( int s ){
	FILE *fp; // pointer to current directory
	char *buf;
	char line[MAX_LINE];
	uint16_t len, netlen; // length of message
	int alcnt = 1; // increment size (count allocs)
	long sendlen;

	// new method for continually allocating data
	
	fp = fopen("boards.txt", "r+");
	if (fp == NULL){
		len = 0;
		netlen = htons( len );
		if( write( s, &netlen, sizeof(uint16_t) ) == -1 ){
			fprintf( stderr, "myfrmd: error sending length\n" );
			exit(-1);
		}
		return;
	}

	buf = malloc(sizeof(char)*MAX_LINE);
	buf[0] = '\0';

	while(fgets(line, MAX_LINE, fp) != NULL) {
		if (MAX_LINE*alcnt <= (strlen(buf) + strlen(line) + 1)) {
			// size continually increments by the data added
			buf = realloc(buf, sizeof(char)*MAX_LINE*(++alcnt));
		}
		strcat(buf, line);
		// allocate more memory
	} // buffer keeps growing on the heap until we can send it
	
	if (fclose(fp) == -1) {
		fprintf( stderr, "myfrmd: could not close directory\n" );
	}

	//send message length
	len = strlen(buf) + 1;
	netlen = htons( len );
	if( write( s, &netlen, sizeof(uint16_t) ) == -1 ){
		fprintf( stderr, "myfrmd: error sending length\n" );
		free( buf );
		exit(-1);
	}

	// loop through for large strings
	long i;
	for (i = 0; i < len; i+= MAX_LINE) { 
		// check cases where we need to send more than one part
		sendlen = (len-i < MAX_LINE) ? len-i : MAX_LINE;
		if( write(s, buf + i, sendlen) == -1 ){
			fprintf( stderr, "myfrmd: error sending directory listing\n" );
			free( buf );
			exit(-1);
		}
	}
	// client can concatenate all these strings and stop listening once the length equals the announced length

	free(buf);
}

void read_board( int s ){
	char* fileName;
	long fileLen;
	long fileLenNet, sendlen;
	char line[MAX_LINE+1];
	char *buf;
	int alcnt = 1;
	FILE* fp;

	if (receive_instruction(s, &fileName) <= 0) {
		fprintf( stderr, "myfrmd: instruction receive error\n" );
		free( fileName );
		return;
	}

	fp = fopen(fileName, "r+");
	if (fp == NULL) {
		fileLen = -1;
	// send the file size to the client
	} else {
		buf = malloc(sizeof(char)*MAX_LINE);
		buf[0] = '\0';
		while(fgets(line, MAX_LINE, fp)!= NULL && !feof(fp)) {
			if (MAX_LINE*alcnt <= (strlen(buf) + strlen(line) + 1)) {
				// size continually increments by the data added
				buf = realloc(buf, sizeof(char)*MAX_LINE*(++alcnt));
			}

			if (feof(fp)) break;

			strcat(buf, line);
			
		} // buffer keeps growing on the heap until we can send it

		fileLen = strlen(buf)+1;	
	}
	free(fileName);

	//send message length
	fileLenNet = htonl( fileLen );
	if( write( s, &fileLenNet, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrmd: error sending length\n" );
		free( buf );
		exit(-1);
	}

	// we're done if the file didn't exist or is empty
	if(fileLen == -1 || fileLen == 0 ){
		return;
	}

	if (fclose(fp) == -1) {
		fprintf( stderr, "myfrmd: could not close directory\n" );
	}

	// loop through for large strings
	long i;
	for (i = 0; i < fileLen; i+= MAX_LINE) { 
		// check cases where we need to send more than one part
		sendlen = (fileLen-i < MAX_LINE) ? fileLen-i : MAX_LINE;
		if( write(s, buf + i, sendlen) == -1 ){
			fprintf( stderr, "myfrmd: error sending directory listing\n" );
			free( buf );
			exit(-1);
		}
	}
	// client can concatenate all these strings and stop listening once the length equals the announced length

	free(buf);
}

void append_file( int s ){

	char boardName[MAX_LINE];
	char fileName[MAX_LINE];
	char msg[1];
	//char *buf;
	char *fileText;
	FILE *fp = NULL; // file pointer to write to
	long i;
	long len, netlen, recvlen;
	char c;

	// get board and fileName
	addr_len = sizeof(struct sockaddr);
	if( recvfrom( s_d, boardName, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrmd: error receiving board name\n" );
		exit( 1 );
	}
	if( recvfrom( s_d, fileName, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrmd: error receiving file name\n" );
		exit( 1 );
	}

	bool board;
	bool file;
	if( (board = check_board(boardName)) && !(file = check_board_file( boardName, fileName )) ){
		msg[0] = 'y';
	} else {
		msg[0] = 'n';
	}

	if( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
		fprintf( stderr, "myfrmd: error sending validation board exists, file does not\n" );
		exit( 1 );
	}
	
	if (msg[0] == 'n') // board did not exist or file did
		return;

	if( recvfrom(s_d, msg, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrmd: error receiving confirmation file exists\n" );
		return;
	};

	if( msg[0] == 'n' )
		return;

	// get file name	
	
	// open the file to write to
	char newFile[MAX_LINE*2 + 1];
	strcpy(newFile, boardName);
	strcat(newFile, "-");
	strcat(newFile, fileName); // boardName-fileName
	fp = fopen(newFile, "w+");
	if (fp == NULL) {
		fprintf( stderr, "myftdp: could not create file\n");
		msg[0] = 'n';
	} else {
		msg[0] = 'y'; // ready 
		// tell client we are ready
	}	
	if ( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
		fprintf( stderr, "myfrmd: error sending confirmation server ready \n" );
		exit( 1 );
	}
	
	if (msg[0] == 'n') 
		return;

	if( read( s, &len, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrmd: size receive error\n" );
		return;
	}
	len = ntohl( len );

	fileText = malloc( len );
	for( i = 0; i < len; i += MAX_LINE ){
		recvlen = (len - i < MAX_LINE ) ? len-i : MAX_LINE;
		if( read( s, fileText+i, recvlen ) == -1 ){
			fprintf( stderr, "myfrmd: error receiving file data \n" );
			free( fileText );
			return;
		}
	}

	// write the file
	for (i = 0; i < len; i++) {
		c = fileText[i]; // get every character
		fputc( c, fp ); 
	}
	fclose( fp );

	add_file_name(boardName, fileName);

	free( fileText );

}

void download_file( int s ){

	char boardName[MAX_LINE];
	char fileName[MAX_LINE];
	struct stat fileStats;
	char msg[1];
	//char *buf;
	char *fileText;
	FILE *fp = NULL; // file pointer to write to
	long i;
	long fileLen, sendlen, net_size;
	char c;

	// get board and fileName
	addr_len = sizeof(struct sockaddr);
	if( recvfrom( s_d, boardName, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrmd: error receiving board name\n" );
		exit( 1 );
	}
	if( recvfrom( s_d, fileName, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrmd: error receiving file name\n" );
		exit( 1 );
	}

	bool board;
	bool file;
	char newFile[MAX_LINE*2 + 1];
	strcpy(newFile, boardName);
	strcat(newFile, "-");
	strcat(newFile, fileName); // boardName-fileName
	if( (board = check_board(boardName)) && (file = check_board_file( boardName, fileName )) ){
		if( access( newFile, F_OK ) != -1 ) {
			stat(newFile, &fileStats);
			fileLen = fileStats.st_size;
		} else{
			fileLen = 0;
		}

	} else {
		fileLen = 0;
	}
	
	// send the length of the file 
	net_size = htonl( fileLen );
	if( write( s, &net_size, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrm: error sending size\n" );
		return;
	}

	if (fileLen <= 0) // no file to send, return
		return;


	fileText = malloc( fileLen );
	fp = fopen(newFile, "r");
	for ( i = 0; i < fileLen; i++) {
		c = fgetc( fp );
		fileText[i] = c;
	}
	fclose( fp );

	for (i = 0; i < fileLen; i += MAX_LINE) {
		sendlen = (fileLen-i < MAX_LINE) ? fileLen-i : MAX_LINE;
		if( write( s, fileText+i, sendlen ) == -1 ){
			fprintf( stderr, "myfrm: error sending file data\n" );
			free( fileText );
			return;
		}
		// some timing issue resolved by waiting a little
		// NOT NEEDED LOCALLY
	}

	free( fileText );


}

void destroy_board( int s ){
	char board_name[MAX_LINE];
	bool board_exists;
	char result[1];

	// get the board name from the client
	addr_len = sizeof( struct sockaddr );
	if( recvfrom( s_d, board_name, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) == -1 ){
		fprintf( stderr, "myfrmd: upd receive error\n" );
		return;
	}

	// make sure the board exists
	board_exists = check_board( board_name );

	if( board_exists ){
		delete_board_name( board_name );
		remove( board_name );
		result[0] = 0;
	}
	else{
		result[0] = 1;
	}

	if( sendto( s_d, result, 1, 0, (struct sockaddr*)&s_in, sizeof( struct sockaddr ) ) < 0 ){
		fprintf( stderr, "myfrmd: error sending result\n" );
	}
}

void send_result(int s, short result) {
	uint16_t netresult = htons( result );
	if( write( s, &netresult, sizeof(uint16_t) ) == -1 )
		fprintf( stderr, "myfrmd: error sending result to client" );
}

int receive_instruction(int s, char** buf) {
	int i;
	long len, recvlen;

	if( read( s, &len, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrmd: size receive error\n" );
		return -1;
	}
	len = ntohl( len );

	*buf = malloc( len );

	for( i = 0; i < len; i += MAX_LINE ){
		recvlen = (len - i < MAX_LINE ) ? len-i : MAX_LINE;
		if( read( s, (*buf)+i, recvlen ) == -1 ){
			fprintf( stderr, "myfrmd: error receiving instruction\n" );
			return -1;
		}
	}

	return len;	
}
