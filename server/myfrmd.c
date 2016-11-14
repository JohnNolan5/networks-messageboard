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
void delete_message(int);
void edit_message(int);
void list_boards(int);
void read_board(int);
void append_file(int);
void download_file(int);
void destroy_board(int);

void request(int);
int list_dir(int);
void remove_dir(int);
void make_dir(int);
void delete_file(int);
void change_dir(int);
int check_dir(char*); // checks that the directory exists
int check_file(char*);
int receive_instruction(int,char**);
void send_result(int, short);
void upload(int);
int receive_file(int,FILE**); 

int main( int argc, char* argv[] ){
	struct sockaddr_in sin, client_addr;
	char buf[MAX_LINE], username[MAX_LINE], password[MAX_LINE];
	FILE* fp;
	int s, s_d, new_s, len, opt;
	uint16_t port;
	int response; // response to send back to 
	/*char timestamp[1024];
	struct timeval t_of_day;
	struct tm* local_t;
	time_t raw_t;*/

	// validate and put command line in variables
	if( argc == 2 ){
		port = atoi(argv[1]);
	} else {
		fprintf( stderr, "usage: myfrmd port\n" );
		exit( 1 );
	}

	// build address data structure
	bzero( (char*)&sin, sizeof( sin ) );
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;// use default IP address
	sin.sin_port = htons( port );

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

	//TODO udp socket options?
	// bind the created socket to the address
	if( bind( s, (struct sockaddr*)&sin, sizeof( sin ) ) < 0 ){
		fprintf( stderr, "myfrmd: socket binding error\n" );
		exit( 1 );
	}

	// bind the udp socket to the address
	if( bind( s_d, (struct sockaddr*)&sin, sizeof( sin ) ) < 0 ){
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
		if( ( new_s = accept( s, (struct sockaddr*)&sin, &len ) ) < 0 ){
			fprintf( stderr, "myfrmd: accept error\n" );
			exit( 1 );
		}
		
		send_result(new_s, 1); // request username
		
		if (read(new_s, username, MAX_LINE) == -1) {
			fprintf( stderr, "myfrmd: error receiving username\n");
			exit( 1 );
		}

		send_result(new_s, 2); // request password 
		// should these be different request numbers?
		
		if (read(new_s, password, MAX_LINE) == -1) {
			fprintf( stderr, "myfrmd: error receiving password \n");
			exit( 1 );
		}
	
		// TODO: make a function processing files and checking for strings
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
		printf("attempting getline \n");
		while (getline(&user_line, &len, fp) != -1) {
			//if (((user_found = strstr(user_line, username)) != NULL) && (strlen(user_found) == strlen(username))) { 
			if (strlen(user_line) <= 0) continue;
			user_test = strtok(user_line, " \n");
			printf("user_test: %s\n", user_test);
			if (strcmp(user_test, username) == 0) {
// username in line and s
				user_exists = true;	
				pass_test = strtok(NULL, " \n");
				printf("pass_test: %s\n", pass_test);
				if ((strcmp(pass_test, password) == 0)) {
					send_result(new_s, 1); // Signed in! proceed...
					signed_in = true;
				}
			}
			memset(user_line,0,strlen(user_line));
			memset(user_test,0,strlen(user_test));
			memset(pass_test,0,strlen(pass_test));
		}
		printf("ended getline \n");
		if (!user_exists) {
			// append username and password to file
			//printf("%s\n", username);
			fprintf(fp, "%s %s\n", username, password);
			fclose(fp);
			signed_in = true;
			printf("got line, signed in\n");
			send_result(new_s, 1);
		} else if (!signed_in) {
			// not able to sign in
			//printf("could not sign in\n");
			send_result(new_s, -1); // either password wrong or need a new user name
			fclose(fp);
		}
		printf( "signed in : %i\n", signed_in);
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
	fp = fopen("boards.txt", "ra+");
	if (fp == NULL) {
		fp = fopen("boards.txt", "w+");
		// no boards existing, tell it its new
		if (fp == NULL) {
		fprintf( stderr, "myfrmd: could not open boards file\n");
		send_result(s, -1); // tell client?
		return;
		}
	}
	
	if (check_board(board_name)) {
		send_result(s, -2);
		return;
	}
/*
	while (getline(&board_line, &len, fp) != -1) {

			if (len <= 0) continue;

			board_test = strtok(board_line, " \n");

			if (strcmp(board_test, board_name) == 0) {
				send_result(s, -2); // board exists
				return;
			}
		memset(board_line, 0, strlen(board_line));
		memset(board_test, 0, strlen(board_test));
	}
*/
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
	
	printf("Adding \"%s\" to \"%s\"\n", message, fileName);
	board_exists = check_board(fileName);

	if (board_exists) {
		printf("board exists");
		fp = fopen(fileName, "a+");
		if (fp == NULL) {
			fprintf(stderr, "myfrmd: Trouble opening the board\n");
			send_result(s, -2);
			free(fileName);
			free(message);
			return;
		}
		fprintf(fp, "%s\n%s\n\n", message, username);
		fclose(fp);
		send_result(s, 1);

	} else {
		send_result(s, -1); // file does not exist
	}
	//TODO: mapy we should call strcpy on receive_instruction reference so we dont keep allocating to and freeing it outside. 
	free(fileName);
	free(message);

}

bool check_board(const char* board_name) {
	
	char *board_line = NULL;
	char *board_test;
	FILE *fp;
	size_t len;

	printf("checking boards.txt\n");
	fp = fopen("boards.txt", "r+");

	if (fp == NULL) {
		return false;
	}
	
	printf("getting board_line\n");
	while (getline(&board_line, &len, fp) != -1) { //SEGFAULT

		printf("check: %s\n", board_line);
		if (strlen(board_line) <= 0) continue;

		board_test = strtok(board_line, " \n"); // to check for files in the future use strtok(NULL, " \n");

		if (strcmp(board_test, board_name) == 0) {
		// filename found
			fclose(fp);
			return true;
		}
	}
	
	fclose(fp);

	return false;
}

void delete_message( int s ){

}

void edit_message( int s ){

}

void list_boards( int s ){

	FILE *fp; // pointer to current directory
	char *buf;
	char *line;
	uint16_t len, netlen; // length of message
	int alcnt = 1; // increment size (count allocs)
	long sendlen;

	// new method for continually allocating data
	
	fp = fopen("boards.txt", "r+");
	if (fp == NULL){
		fprintf( stderr, "myfrmd: error opening boards file\n" );
		exit(-1);
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
	int i;
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
	//char c;
	char* fileName;
//	char* fileText;
//	uint16_t result;
	long fileLen;
	long fileLenNet, sendlen;
	//struct stat fileStats;
	//MHASH compute;
	//char hash[16];
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
		printf("Entering while loop\n");
		while(fgets(line, MAX_LINE, fp)!= NULL && !feof(fp)) {
			printf("Checking length\n");
			if (MAX_LINE*alcnt <= (strlen(buf) + strlen(line) + 1)) {
				// size continually increments by the data added
				buf = realloc(buf, sizeof(char)*MAX_LINE*(++alcnt));
			}

			if (feof(fp)) break;

			printf("concatenating\n");

			strcat(buf, line);
			
		} // buffer keeps growing on the heap until we can send it

		fileLen = strlen(buf)+1;	
	}
	free(fileName);

	printf("Sending length: %li\n", fileLen);
	//send message length
	fileLenNet = htonl( fileLen );
	if( write( s, &fileLenNet, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrmd: error sending length\n" );
		free( buf );
		exit(-1);
	}

	printf("checking len.");
	// we're done if the file didn't exist or is empty
	if(fileLen == -1 || fileLen == 0 ){
		return;
	}

	if (fclose(fp) == -1) {
		fprintf( stderr, "myfrmd: could not close directory\n" );
	}

	// loop through for large strings
	int i;
	for (i = 0; i < fileLen; i+= MAX_LINE) { 
		// check cases where we need to send more than one part
		sendlen = (fileLen-i < MAX_LINE) ? fileLen-i : MAX_LINE;
		printf("writing.");
		if( write(s, buf + i, sendlen) == -1 ){
			fprintf( stderr, "myfrmd: error sending directory listing\n" );
			free( buf );
			exit(-1);
		}
	}
	printf("done writing.");
	// client can concatenate all these strings and stop listening once the length equals the announced length

	free(buf);
}

void append_file( int s ){

}

void download_file( int s ){

}

void destroy_board( int s ){

}

void request( int s ){
	char c;
	char* fileName;
	char* fileText;
	uint16_t result;
	long fileLen;
	long fileLenNet, sendlen, i;
	struct stat fileStats;
	MHASH compute;
	char hash[16];
	FILE* fp;

	if (receive_instruction(s, &fileName) <= 0) {
		fprintf( stderr, "myfrmd: instruction receive error\n" );
		free( fileName );
		return;
	}

	result = check_file( fileName );

	// file exists: get file length
	if( result == 1 ){
		stat( fileName, &fileStats );
		fileLen = fileStats.st_size;

	// file doesn't exist: return result
	} else {
		fileLen = -1;
	}

	// send the file size to the client
	fileLenNet = htonl( fileLen );
	if( write( s, &fileLenNet, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrmd: error sending file size\n" );
		free( fileName );
		return;
	}

	// we're done if the file didn't exist or is empty
	if( result != 1 || fileLen == 0 ){
		free( fileName );
		return;
	}

	// initialize hash computation
	compute = mhash_init( MHASH_MD5 );
	if( compute == MHASH_FAILED ){
		fprintf( stderr, "myfrmd: hash failed\n" );
		free( fileName );
		return;
	}

	// get ready to read file
	fileText = malloc( fileLen );
	fp = fopen( fileName, "r" );
	free( fileName );

	// actually read the file
	for( i = 0; i < fileLen; i++ ){
		c = fgetc( fp );
		fileText[i] = c;
		mhash( compute, &c, 1 );
	}
	fclose( fp );

	// end hash computation and send to client
	mhash_deinit( compute, hash );
	if( write( s, hash, 16 ) == -1 ){
		fprintf( stderr, "myfrmd: error sending hash\n" );
		free( fileText );
		return;
	}

	// send the file's data to the client
	for( i=0; i<fileLen; i+= MAX_LINE ){
		sendlen = (fileLen - i < MAX_LINE) ? fileLen-i : MAX_LINE;
		if( write( s, fileText+i, sendlen ) == -1 ){
			fprintf( stderr, "myfrmd: error sending file data\n" );
			free( fileText );
			return;
		}
		// some timing issue resolved by waiting a little
		usleep( 200 );
	}

	free( fileText );
}

void upload( int s ) {
	short result;
	int rec_result;
	char *buf;
	char *fileText;
	FILE *fp = NULL; // file pointer to write to
	int namelen; // file name length
	int i;
	long len, recvlen;
	char hash[16], hashCmp[16];
	char c;
	long upload_time, netuplt;
	struct timeval start, stop;
	MHASH compute;

	if( read( s, &result, sizeof(short) ) == -1 ){
		fprintf( stderr, "myfrmd: error receiving confirmation file exists\n" );
		return;
	}
	result = ntohs( result );

	if( !result )
		return;

	// get file name
	if ( receive_instruction(s, &buf) <= 0 ) {
		fprintf( stderr, "myfrmd: error receiving file name\n" );
		return;
	}
	
	// open the file to write to
	fp = fopen(buf, "w+");
	if (fp == NULL) {
		fprintf( stderr, "myftdp: could not create file\n");
		result = -1; // not ready
		send_result(s, result);
		return;
	} else {
		result =  1; // ready 
		// tell client we are ready
		send_result(s, result);
	}	
	
	if( read( s, &len, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrmd: size receive error\n" );
		return;
	}
	len = ntohl( len );

	fileText = malloc( len );
	gettimeofday(&start, NULL);
	for( i = 0; i < len; i += MAX_LINE ){
		recvlen = (len - i < MAX_LINE ) ? len-i : MAX_LINE;
		if( read( s, fileText+i, recvlen ) == -1 ){
			fprintf( stderr, "myfrmd: error receiving instruction\n" );
			free( fileText );
			return;
		}
	}
	gettimeofday(&stop, NULL);

	upload_time = 1000000*stop.tv_sec + stop.tv_usec - 1000000*start.tv_sec - start.tv_usec;
	
	// get hash from client
	if( read( s, hash, 16 ) == -1 ){
		fprintf( stderr, "myftp: error receiving md5 hash\n" );
		free( fileText );
		return;
	}

	// initialize hash computation
	compute = mhash_init( MHASH_MD5 );
	if( compute == MHASH_FAILED ){
		fprintf( stderr, "myfrmd: hash failed\n" );
		free( buf );
		return;
	}

	// write the file
	for (i = 0; i < len; i++) {
		c = fileText[i]; // get every character
		fputc( c, fp ); 
		mhash( compute, &c, 1 );
	}
	fclose( fp );

	printf( "upload time: %ld\n", upload_time );
	
	// get hash
	mhash_deinit( compute, hashCmp );
	if ( strncmp( hash, hashCmp, 16 )) { // hash unsuccessful
		// set to -1 if there is an error
		upload_time = -1;
	}
	netuplt = htonl( upload_time );

	printf( "hash: " );
	for( i=0; i<16; i++ )
		printf( "%.2x", hashCmp[i] );
	printf( "\n" );

	if( write( s, &netuplt, sizeof(long) ) == -1 )
		fprintf( stderr, "myfrmd: error sending result to client" );

	free( fileText );
}

void delete_file(int s) {
	char* file;
	char* confirm;
	uint16_t result, netresult;

	if (receive_instruction(s, &file) <= 0) {
		fprintf( stderr, "myfrmd: instruction receive error\n" );
		free( file );
		return;
	}

	result = check_file( file );

	netresult = htons( result );
	// send response regarding file existence
	if (send(s, &netresult, sizeof(uint16_t), 0) == -1) {
		fprintf( stderr, "myfrmd: error sending file status\n");
		free( file );
		return;
	}

	// file exists
	if( result == 1 ){
		// get confirmation from client
		if( receive_instruction( s, &confirm ) <= 0 ){
			fprintf( stderr, "myfrmd: error receiving client confirmation\n" );
			free( file );
			return;
		}

		if (!strncmp(confirm, "Yes", 3)) {
			// success
			if( !remove( file ) )
				result = 1;
			// otherwise failure
			else
				result = -1;

		}
	}
	send_result(s, result);

	free( file );
}


void remove_dir(int s) {
	char* dir;
	char* confirm;
	uint16_t result, netresult;

	if (receive_instruction(s, &dir) <= 0) {
		fprintf( stderr, "myfrmd: instruction receive error\n" );
		free( dir );
		return;
	}

	result = check_dir(dir);

	netresult = htons( result );
	// send response regarding directory
	if (send(s, &netresult, sizeof(uint16_t), 0) == -1) {
		fprintf( stderr, "myfrmd: error sending directory status\n");
		free( dir );
		return;
	}

	// directory exists
	if( result == 1 ){
		// get confirmation from client
		if( receive_instruction( s, &confirm ) <= 0 ){
			fprintf( stderr, "myfrmd: error receiving client confirmation\n" );
			free( dir );
			return;
		}

		if (!strncmp(confirm, "Yes", 3)) {
			// success
			if( rmdir( dir ) == 0 )
				result = 1;
			// otherwise failure
			else
				result = -1;

			netresult = htons( result );
			if( write( s, &netresult, sizeof(uint16_t) ) == -1 )
				fprintf( stderr, "myfrmd: error sending result to client" );
		}
	}
	free( dir );
}

// checks that file exists
int check_file(char *file) {
	// exists
	if( access( file, F_OK ) != -1 )
		return 1;
	// doesn't exist
	else
		return -1;
}

int check_dir(char *dir) { // checks that the directory exists

	DIR *dp; // pointer to current directory
	strtok(dir, "\n");
	dp = opendir(dir); // open working directory
	if (dp == NULL){
		//fprintf( stderr, "myfrmd: error opening directory\n" );
		return -1;
	} else {
		closedir(dp);
		return 1; //directory found
	}
}



void make_dir(int s) {
	char* dir; 
	struct stat st = {0}; // holds directory status
	short result;

	if( receive_instruction( s, &dir ) <= 0 ){
		fprintf( stderr, "myfrmd: instruction receive error\n" );
		free( dir );
		return;
	}

	// directory doesn't exist
	if( check_dir( dir ) == -1 ){
		// successful mkdir
		if( !mkdir( dir, 0700 ) )
			result = 1;
		// otherwise failure to create dir
		else
			result = -1;
	// directory exists
	} else
		result = -2;
	
	send_result(s, result);

	free( dir );
}

void change_dir(int s) {
	char* dir;
	short result;
	uint16_t netresult;

	if (receive_instruction(s, &dir) <= 0)
		result = -1;
	else if (check_dir(dir) == -1) // directory not found
		result = -2;
	else if( chdir( dir ) == 0 ) // success // TODO: remove new line, might replace with strtok(dir, "\n");
		result = 1;
	else
		result = -1;

	send_result(s, result);
/*
	netresult = htons( result );
	if( write( s, &netresult, sizeof(uint16_t) ) == -1 )
		fprintf( stderr, "myfrmd: error sending result to client" );

*/
	free( dir );
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
