// myfrm.c
// John Riordan
// And Nolan
// jriorda2
// jnolan5

// usage: myfrm server_name port

#include "myfrm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_LINE 256

int s_d, s, addr_len;
struct sockaddr_in s_in;

int main( int argc, char* argv[] ){
	FILE* fp = NULL;
	struct hostent* hp;
	char* host;
	char buf[MAX_LINE+1];
	int i, len;
	uint16_t port;
	char msg[1];
	/*struct timeval start, end;*/

	addr_len = sizeof( struct sockaddr );
	// validate and put command line in variables
	if( argc == 3 ){
		host = argv[1];
		port = atoi(argv[2]);
	} else {
		fprintf( stderr, "usage: myfrm server_name port\n" );
		exit( 1 );
	}

	// translate host name into peer's IP address
	hp = gethostbyname( host );
	if( !hp ){
		fprintf( stderr, "myfrm: unknown host: %s\n", host );
		exit( 1 );
	}

	// build address data structure
	bzero( (char*)&s_in, sizeof( s_in ) );
	s_in.sin_family = AF_INET;
	bcopy( hp->h_addr, (char*)&s_in.sin_addr, hp->h_length );
	s_in.sin_port = htons( port );

	// create the socket
	if( (s = socket( PF_INET, SOCK_STREAM, 0 )) < 0 ){
		fprintf( stderr, "myfrm: socket creation error\n" );
		exit( 1 );
	}

	// create the udp socket
	if ( (s_d = socket( PF_INET, SOCK_DGRAM, 0 )) < 0) {
		fprintf( stderr, "myfrm: udp socket creation error\n" );
		exit( 1 );
	}
	// connect the created socket to the remote server
	if( connect( s, (struct sockaddr*)&s_in, sizeof(s_in) ) < 0 ){
		fprintf( stderr, "myfrm: connect error\n" );
		close( s );
		close( s_d );
		exit( 1 );
	}

	if(receive_result(s) == 1) {// get request
		// setup username and password
		printf("Enter your username: ");
		if( fgets( buf, MAX_LINE, stdin ) ){
			int i;
			for( i=0; i<MAX_LINE; i++ ){
				if( buf[i] == '\n' || buf[i] == ' ' ){
					buf[i] = '\0';
				}
			}

			if( sendto( s_d, buf, strlen(buf)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
				fprintf( stderr, "myfrm: error sending username\n" );
				exit( 1 );
			}
		} else {
			fprintf( stderr, "myfrm: error, no username received \n");
			exit( 1 );
		}
	}

	if (receive_result(s) == 2) {// get request
	printf("Enter your password: ");
	if (	fgets( buf, MAX_LINE, stdin ) ) {
		int i;
		for( i = 0; i < MAX_LINE; i++) {
			if (buf[i] == '\n' || buf[i] == ' ') {
				buf[i] = '\0'; 
			}
		}
		if ( send( s, buf, strlen(buf)+1, 0) == -1 ) {// send null character as well
			fprintf( stderr, "myfrm: error sending username\n");
			exit(1);
		}
	} else {
		fprintf( stderr, "myfrm: error, no username received \n");
		exit(1);
	}
	}
	
	short result;
	printf("waiting for result... \n");
	if ((result = receive_result(s)) == 1) {
		printf( "Signed in.\n");
	} else if (result == -1) { //TODO: let users sign in again
		printf( "Password incorrect or username already chosen.\n");
		return;
	} else {
		printf( "invalid sign in response: %hi", result);
		return;
	}

	printf( "Enter your operation (CRT, MSG, DLT, EDT, RDB, LIS, APN, DWN, DST, XIT, or SHT): " );
	// main loop
	while( fgets( buf, MAX_LINE, stdin ) ){

		buf[MAX_LINE] = '\0';
		//TODO: use s_d

		if( send( s, buf, 3, 0 ) == -1 ){
			fprintf( stderr, "myfrm: send error\n" );
			exit( 1 );
		} else {
		if( !strncmp( buf, "XIT", 3 ) ){
			printf( "Bye bye\n" );
			break;
		}
		handle_action(buf, s, s_d);
		}
		printf( "Enter your operation (CRT, MSG, DLT, EDT, RDB, LIS, APN, DWN, DST, XIT, or SHT): " );
	}

	close( s );
	close( s_d );

	return 0;
}

void handle_action(char* msg, int s, int s_d) {
	// check for all special 3 char messages
	if( !strncmp("CRT", msg, 3) )
		create_board( s ); //TODO: need s_d here, should mak a global address struct or somethin
	else if( !strncmp("MSG", msg, 3) )
		leave_message( s ); //TODO: should be s_d
	else if( !strncmp("DLT", msg, 3) ) 
		delete_message( s_d );
	else if( !strncmp("EDT", msg, 3) )
		edit_message( s_d );
	else if( !strncmp("LIS", msg, 3) )
		list_boards( s ); //TODO: need s_d
	else if( !strncmp("RDB", msg, 3) )
		read_board( s );
	else if( !strncmp("APN", msg, 3) )
		append_file( s );
	else if( !strncmp("DWN", msg, 3) )
		download_file( s );
	else if( !strncmp("DST", msg, 3) )
		destroy_board( s );
}

void create_board(int s) {
	short result;
	FILE* fp;

	printf("Enter the name of your board: \n");
	send_name(s);

	result = receive_result(s);
	
	if (result == 1) {
		printf("Successfully created the board\n");
	} else if (result == -1) {
		printf("Error creating the board\n");
	} else if (result == -2) {
		printf("The board you named already exists.\n");
	}
	
}


void leave_message( int s_d ){

	char fileName[MAX_LINE];
	char message[MAX_LINE];
	uint16_t result, resultNet;

	fileName[0] = '\0';
	message[0] = '\0';
	printf( "Enter the name of the board where you want to post a message: " );
	fflush( stdin );
	fgets( fileName, MAX_LINE-1, stdin );
// trim the \n off the end
	if( strlen(fileName) < MAX_LINE )
		fileName[strlen(fileName)-1] = '\0';
	else
		fileName[MAX_LINE-1] = '\0';

	// send the file name over
	send_instruction( s_d, fileName );

	printf( "Enter your message: " );
	fflush( stdin );
	fgets( message, MAX_LINE-1, stdin );
// trim the \n off the end
	if( strlen(message) < MAX_LINE )
		message[strlen(message)-1] = '\0';
	else
		message[MAX_LINE-1] = '\0';

	// send the file name over
	send_instruction( s_d, message );

	
	result = receive_result( s_d);

	// both are -1
	if( result == -1 || result == 4294967295 ){
		printf( "The board does not exist\n" );
	} else if (result == -2) {
		printf( "Server error: board could not open\n");
	} else if (result == 1) {
		printf("Message posted successfully\n");
	} else {
		fprintf(stderr, "myfrm: invalid message posting result\n");
	}
}

void delete_message( int s_d ){
	char boardName[MAX_LINE];
	char msg[1];
	char c;
	char line[MAX_LINE];

	// get board name from user
	boardName[0] = '\0';
	printf( "Enter the name of the board: " );
	fflush( stdin );
	fgets( boardName, MAX_LINE-1, stdin );

	// trim the \n off the end
	if( strlen(boardName) < MAX_LINE )
		boardName[strlen(boardName)-1] = '\0';
	else
		boardName[MAX_LINE-1] = '\0';

	// send board name to server
	addr_len = sizeof( struct sockaddr );
	if( sendto( s_d, boardName, strlen(boardName)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
		fprintf( stderr, "myfrm: error sending board name\n" );
		return;;
	}

	// get confirmation board exists
	addr_len = sizeof(struct sockaddr);
	if( recvfrom( s_d, msg, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrm: error receiving validation board exists\n" );
		return;
	}
	if( msg[0] == 'n' ){
		printf( "board does not exist\n" );
		return;
	}

	while( 1 ){
		// receive line
		addr_len = sizeof(struct sockaddr);
		if( recvfrom( s_d, line, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
			fprintf( stderr, "myfrm: error receiving possible message to delete\n" );
			return;
		}

		// exit on end of board
		if(!strcmp( line, "$" ))
			break;

		// prompt user whether to delete line
		printf( "Do you want to delete the following line:\n%s'y' to delete, anything else to not delete: ", line );
		fflush(stdin);
		c = msg[0] = fgetc( stdin );
		while( c != '\n' )
			c = fgetc( stdin );

		// send response to server
		if( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
			fprintf( stderr, "myfrm: error sending delete confirmation\n" );
			return;
		}
	}
	printf( "reached end of board\n" );
}

void edit_message( int s_d ){
	char boardName[MAX_LINE];
	char newLine[MAX_LINE];
	char msg[1];
	char c;
	char line[MAX_LINE];

	// get board name from user
	boardName[0] = '\0';
	printf( "Enter the name of the board: " );
	fflush( stdin );
	fgets( boardName, MAX_LINE-1, stdin );

	// trim the \n off the end
	if( strlen(boardName) < MAX_LINE )
		boardName[strlen(boardName)-1] = '\0';
	else
		boardName[MAX_LINE-1] = '\0';

	// send board name to server
	addr_len = sizeof( struct sockaddr );
	if( sendto( s_d, boardName, strlen(boardName)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
		fprintf( stderr, "myfrm: error sending board name\n" );
		return;;
	}

	// get confirmation board exists
	addr_len = sizeof(struct sockaddr);
	if( recvfrom( s_d, msg, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrm: error receiving validation board exists\n" );
		return;
	}
	if( msg[0] == 'n' ){
		printf( "board does not exist\n" );
		return;
	}

	while( 1 ){
		// receive line
		addr_len = sizeof(struct sockaddr);
		if( recvfrom( s_d, line, MAX_LINE, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
			fprintf( stderr, "myfrm: error receiving possible message to delete\n" );
			return;
		}

		// exit on end of board
		if(!strcmp( line, "$" ))
			break;

		// prompt user whether to edit line
		printf( "Do you want to edit the following line:\n%s'y' to edit, anything else to not edit: ", line );
		fflush(stdin);
		c = msg[0] = fgetc( stdin );
		while( c != '\n' )
			c = fgetc( stdin );

		// send response to server
		if( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
			fprintf( stderr, "myfrm: error sending edit confirmation\n" );
			return;
		}

		if( msg[0] == 'y' ){
			// get edited line
			newLine[0] = '\0';
			printf( "Enter the edited line:\n" );
			fflush( stdin );
			fgets( newLine, MAX_LINE-1, stdin );
			newLine[MAX_LINE-1] = '\0';

			// send response to server
			if( sendto( s_d, newLine, strlen(newLine)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
				fprintf( stderr, "myfrm: error sending edited line\n" );
				return;
			}
		}
	}
	printf( "reached end of board\n" );
}

void list_boards( int s ){
	char buf[MAX_LINE+1];
	uint16_t len, netlen = 0;
	int i;
	long recvlen;

	buf[MAX_LINE] = '\0';
	if( read( s, &netlen, sizeof(uint16_t) ) == -1 ){
		fprintf( stderr, "myfrm: error receiving listing size\n" );
		return;
	}
	
	len = ntohs( netlen );
	
	if (len == 0) {
		printf("\n"); return;
	}
	
	for( i = 0; i < len; i += MAX_LINE ){
		recvlen = (len - i < MAX_LINE ) ? len-i : MAX_LINE;
		if( read( s, buf, recvlen ) == -1 ){
			fprintf( stderr, "myfrm: error receiving listing\n" );
			return;
		}
		printf( "%s", buf );
	}
}

void read_board( int s ){
	char fileName[MAX_LINE];
	char buf[MAX_LINE+1];
	long fileLen, fileLenNet;
	//FILE* fp;

	printf( "Enter the board name to read: " );
	fflush( stdin );
	fgets( fileName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(fileName) < MAX_LINE )
		fileName[strlen(fileName)-1] = '\0';
	else
		fileName[MAX_LINE-1] = '\0';

	// send the file name over
	send_instruction( s, fileName );

	if( read( s, &fileLenNet, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrm: error receiving listing size\n" );
		return;
	}
	
	fileLen = ntohl( fileLenNet );

	// both are -1
	if( fileLen == -1 || fileLen == 4294967295 ){
		printf( "File does not exist\n" );
		return;
	} else if (fileLen == 0) {
		printf("\n");
		return;
	}

	buf[MAX_LINE] = '\0';
		
	int i;
	for( i = 0; i < fileLen; i += MAX_LINE ){
		if( read( s, buf, MAX_LINE ) == -1 ){
			fprintf( stderr, "myfrm: error receiving listing\n" ); 
			return;
		}
		printf( "%s", buf );
	}

}

void append_file( int s ){
	char fileName[MAX_LINE];
	char boardName[MAX_LINE];
	FILE* fp; // file pointer to read data
	struct stat fileStats;
	long fileLen, net_size, sendlen;
//	long len, netlen;
	//size_t addr_len;
	char* fileText;
	MHASH compute;
	char msg[1];
	char hash[16];
	char c;
	double thrput;
	long i;

	printf( "Enter the name of the board where you will attach your upload: " );
	fflush( stdin );
	fgets( boardName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(boardName) < MAX_LINE )
		boardName[strlen(boardName)-1] = '\0';
	else
		boardName[MAX_LINE-1] = '\0';

	addr_len = sizeof( struct sockaddr );
	if( sendto( s_d, boardName, MAX_LINE, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
		fprintf( stderr, "myfrm: error sending board name\n" );
		return;;
	}

	printf( "Enter the name of the file to upload: " );
	fflush( stdin );
	fgets( fileName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(fileName) < MAX_LINE )
		fileName[strlen(fileName)-1] = '\0';
	else
		fileName[MAX_LINE-1] = '\0';

	addr_len = sizeof( struct sockaddr );
	if( sendto( s_d, fileName, MAX_LINE, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
		fprintf( stderr, "myfrm: error sending file name\n" );
		return;;
	}

	if( recvfrom( s_d, msg, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrm: error receiving validation that board exists\n" );
		return;
	}

	if( msg[0] == 'n' ){
		printf( "The board does not exist, or file already present.\n" );
		return;
	}

	// confirm file exists
	if( access( fileName, F_OK ) != -1 )
		msg[0] = 'y';
	else{
		printf( "The file does not exist\n" );
		msg[0] = 'n';
	}

	if( sendto( s_d, msg, 1, 0, (struct sockaddr*)&s_in, addr_len  ) < 0 ){
		fprintf( stderr, "myfrm: error sending confirmation file exists\n" );
		exit(1);
	}

	if( msg[0] == 'n' )
		return; // file did not exist, done

	
	if( recvfrom( s_d, msg, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrm: error receiving validation server is ready\n" );
		return;
	}

	if( msg[0] == 'n' ){
		printf( "The server is not ready to receive that file.\n" );
		return;
	}
	
	stat(fileName, &fileStats);
	fileLen = fileStats.st_size;

	// send the length of the message
	net_size = htonl( fileLen );

	if( write( s, &net_size, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrm: error sending size\n" );
		return;
	}


	fileText = malloc( fileLen );
	fp = fopen(fileName, "r");
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

	// end hash computation and send to server 
	// DELETED
/*
	// receive response
	if( recvfrom( s_d, msg, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "myfrm: error receiving validation server is ready\n" );
		return;
	}

	if ( msg[0] == 'n') {
		printf( "myfrm: Transfer was unsuccessful\n" );
	} else {
		printf( "Transfer was successful\n" );
	}
*/
}

void download_file( int s ){
	char fileName[MAX_LINE];
	char boardName[MAX_LINE];
	FILE* fp; // file pointer to read data
	struct stat fileStats;
	long len, recvlen;
//	long len, netlen;
	//size_t addr_len;
	char* fileText;
	MHASH compute;
	char msg[1];
	char hash[16];
	char c;
	double thrput;
	long i;

	printf( "Enter the name of the board where you will request the file: " );
	fflush( stdin );
	fgets( boardName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(boardName) < MAX_LINE )
		boardName[strlen(boardName)-1] = '\0';
	else
		boardName[MAX_LINE-1] = '\0';

	addr_len = sizeof( struct sockaddr );
	if( sendto( s_d, boardName, MAX_LINE, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
		fprintf( stderr, "myfrm: error sending board name\n" );
		return;;
	}

	printf( "Enter the name of the file to download: " );
	fflush( stdin );
	fgets( fileName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(fileName) < MAX_LINE )
		fileName[strlen(fileName)-1] = '\0';
	else
		fileName[MAX_LINE-1] = '\0';

	addr_len = sizeof( struct sockaddr );
	if( sendto( s_d, fileName, MAX_LINE, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr) ) < 0 ){
		fprintf( stderr, "myfrm: error sending file name\n" );
		return;
	}
	
	fp = fopen(fileName, "w+");
	if (fp == NULL) {
		fprintf( stderr, "myftdp: could not create file\n");
		return;
	} 
	
	printf("reading length...\n");
	if( read( s, &len, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrmd: size receive error\n" );
		return;
	}
	len = ntohl( len );

	if (len <= 0) {
		printf("The server could not open that file\n");
		fclose(fp);
		return;
	}
	
	fileText = malloc( len );
	for( i = 0; i < len; i += MAX_LINE ){
		recvlen = (len - i < MAX_LINE ) ? len-i : MAX_LINE;
		if( read( s, fileText+i, recvlen ) == -1 ){
			fprintf( stderr, "myfrmd: error receiving file data \n" );
			free( fileText );
			return;
		}
	}

	// hashing removed 

	// write the file
	for (i = 0; i < len; i++) {
		c = fileText[i]; // get every character
		fputc( c, fp ); 
	}
	fclose( fp );

}

void destroy_board( int s ){
	char boardName[MAX_LINE];
	char result[1];

	printf( "Enter the board name to destroy: " );
	fflush( stdin );
	fgets( boardName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(boardName) < MAX_LINE )
		boardName[strlen(boardName)-1] = '\0';
	else
		boardName[MAX_LINE-1] = '\0';

	if( sendto( s_d, boardName, strlen(boardName)+1, 0, (struct sockaddr*)&s_in, sizeof(struct sockaddr)) == -1 ){
		fprintf( stderr, "myfrm: udp send error\n" );
		return;
	}

	if( recvfrom( s_d, result, 1, 0, (struct sockaddr*)&s_in, &addr_len ) < 0 ){
		fprintf( stderr, "error receiving result\n" );
		return;
	}
	
	if( !result[0] ){
		printf( "board deleted successfully\n" );
	}else{
		printf( "failed to delete board\n" );
	}
}

void request( int s ){
	char c;
	char fileName[MAX_LINE];
	char hash[16], hashCmp[16];
	double thrput, upload_time;
	long fileLen;
	FILE* fp;
	char* fileText;
	long recvlen, i;
	struct timeval start, stop;
	MHASH compute;

	printf( "Enter the file name to request: " );
	fflush( stdin );
	fgets( fileName, MAX_LINE, stdin );
// trim the \n off the end
	if( strlen(fileName) < MAX_LINE )
		fileName[strlen(fileName)-1] = '\0';
	else
		fileName[MAX_LINE-1] = '\0';

	// send the file name over
	send_instruction( s, fileName );

	// check if file existed on server
	fileLen = receive_result32( s );

	// both are -1
	if( fileLen == -1 || fileLen == 4294967295 ){
		printf( "File does not exist\n" );
		return;
	}

	// empty file case
	if( fileLen == 0 ){
		fp = fopen( fileName, "w+" );
		fclose( fp );
		return;
	}

	// get hash from server
	if( read( s, hash, 16 ) == -1 ){
		fprintf( stderr, "myfrm: error receiving md5 hash\n" );
		return;
	}

	// get the file's text from the server
	fileText = malloc( fileLen );
	gettimeofday(&start, NULL);
	for( i = 0; i < fileLen; i += MAX_LINE ){
		recvlen = (fileLen - i < MAX_LINE ) ? fileLen-i : MAX_LINE;
		if( read( s, fileText+i, recvlen ) == -1 ){
			fprintf( stderr, "myfrm: error receiving file data\n" );
			free( fileText );
			return;
		}
	}

	gettimeofday(&stop, NULL);

	upload_time = stop.tv_sec - start.tv_sec + ((double)stop.tv_usec)/1000000 - ((double)start.tv_usec)/1000000;

	if (upload_time != 0) {
		thrput = ((double)fileLen)/upload_time;
	} else {
		thrput = 0;
	}
	// initialize hash computation
	compute = mhash_init( MHASH_MD5 );
	if( compute == MHASH_FAILED ){
		fprintf( stderr, "myfrmd: hash failed\n" );
		free( fileText );
		return;
	}
	
	// create and write the file
	fp = fopen( fileName, "w+" );
	for( i = 0; i < fileLen; i++ ){
		c = fileText[i];
		fputc( c, fp );
		mhash( compute, &c, 1 );
	}
	fclose( fp );
	free( fileText );

	mhash_deinit( compute, hashCmp );
	if( !strncmp( hash, hashCmp, 16 ) ){
		printf( "Transfer was successful\n" );
		printf("%ld bytes transferred in %.2f seconds: %.3f Megabytes per second\n", fileLen, ((float)fileLen)/((float)thrput),(float)thrput/1000000.0);
		printf( "File MD5sum: " );
		for (i = 0; i < 16; i++) {
			printf("%0x", hash[i]);
		}
		printf( "\n" );
		int j;
	} else
		printf( "Transfer was unsuccessful\n" );
}

void upload( int s ) {
	char fileName[MAX_LINE];
	short result, net_result;
	FILE* fp; // file pointer to read data
	struct stat fileStats;
	long fileLen, net_size, sendlen;
	char* fileText;
	MHASH compute;
	char hash[16];
	char c;
	double thrput;
	long i;

	printf( "Enter the file name to upload: " );
	fflush( stdin );
	fgets( fileName, MAX_LINE, stdin );

	// trim the \n off the end
	if( strlen(fileName) < MAX_LINE )
		fileName[strlen(fileName)-1] = '\0';
	else
		fileName[MAX_LINE-1] = '\0';

	// confirm file exists
	if( access( fileName, F_OK ) != -1 )
		result = 1;
	else{
		printf( "The file does not exist\n" );
		result = 0;
	}
	net_result = htons( result );

	if( write( s, &net_result, sizeof(short) ) == -1 ){
		fprintf( stderr, "myfrm: error sending confirmation file exists\n" );
		return;
	}

	if( !result )
		return;

	send_instruction( s, fileName );
	
	stat(fileName, &fileStats);
	fileLen = fileStats.st_size;
	result = receive_result( s );
	if( result != 1 ){ // not ready to receive for some reason
		printf( "The server is not ready to receive that file.\n" );
		return;
	}

	// send the length of the message
	net_size = htonl( fileLen );

	if( write( s, &net_size, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrm: error sending size\n" );
		return;
	}

	// initialize hash computation
	compute = mhash_init( MHASH_MD5 );
	if( compute == MHASH_FAILED ){
		fprintf( stderr, "myfrmd: hash failed\n" );
		return;
	}

	fileText = malloc( fileLen );
	fp = fopen(fileName, "r");
	for ( i = 0; i < fileLen; i++) {
		c = fgetc( fp );
		fileText[i] = c;
		mhash( compute, &c, 1);
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
		usleep( 7000 );
	}
	free( fileText );

	// end hash computation and send to server 
	mhash_deinit( compute, hash );
	if( write( s, hash, 16 ) == -1 ){
		fprintf( stderr, "myfrmd: error sending hash\n" );
		return;
	}

	printf( "hash: " );
	for( i=0; i<16; i++ )
		printf( "%.2x", hash[i] );
	printf( "\n" );

	// receive response
	long upload_time;

	upload_time = receive_result32( s );
	// signed and unsigned -1
	if( upload_time == -1 || upload_time == 4294967295 ){
		printf( "myfrm: Transfer was unsuccessful\n" );
	} else {
		printf( "Transfer was successful\n" );
		thrput = ((double)fileLen)/upload_time;
		printf("%ld bytes transferred in %.2f seconds: %.3f Megabytes per second\n", fileLen, ((double)upload_time)/1000000, thrput);
		printf("File MD5sum: %x\n", hash);
	}

}

void delete_file(int s){
	char buf[MAX_LINE];
	short result;

	// buf = malloc( sizeof(char)*MAX_LINE );

	printf( "Enter the file name to remove: " );
	fflush( stdin );
	fgets( buf, MAX_LINE, stdin );

	// trim the \n off the end of buf
	buf[strlen(buf)-1] = 0;

	send_instruction( s, buf );

	result = receive_result( s );
	if( result == 1 ){
		buf[0] = '\0';

		while( strncmp( buf, "Yes", 3 ) && strncmp( buf, "No", 2 ) ){
			printf( "Confirm you want to delete the file: \"Yes\" to delete, \"No\" to ignore: " );
			fflush( stdin );
			fgets( buf, MAX_LINE, stdin );
		}

		send_instruction( s, buf );
		
		if( !strncmp( buf, "No", 2 ) )
			printf( "Delete abandoned by the user!\n" );
		else{
			result = receive_result( s );
			if( result == 1 )
				printf( "File deleted\n" );
			else if( result == -1 )
				printf( "Failed to delete file\n" );
		}
	} else if( result == -1 )
		printf( "The file does not exist on the server\n" );
}


void remove_dir(int s) {
	char buf[MAX_LINE];
	short result;

	printf( "Enter the directory name to remove: " );
	fflush( stdin );
	fgets( buf, MAX_LINE, stdin );
	send_instruction( s, buf );

	result = receive_result( s );
	if( result == 1 ){
		buf[0] = '\0';

		while( strncmp( buf, "Yes", 3 ) && strncmp( buf, "No", 2 ) ){
			printf( "Confirm you want to delete the directory: \"Yes\" to delete, \"No\" to ignore: " );
			fflush( stdin );
			fgets( buf, MAX_LINE, stdin );
		}

		send_instruction( s, buf );
		
		if( !strncmp( buf, "No", 2 ) )
			printf( "Delete abandoned by the user!\n" );
		else{
			result = receive_result( s );
			if( result == 1 )
				printf( "Directory deleted\n" );
			else if( result == -1 )
				printf( "Failed to delete directory\n" );
		}
	} else if( result == -1 )
		printf( "The directory does not exist on the server\n" );
	else
		printf( "WTF: %d\n", result );
}

void change_dir(int s) {
	char buf[MAX_LINE];
	short result;
	printf( "Enter the directory name: " );
	fflush( stdin );
	fgets( buf, MAX_LINE, stdin );

	send_instruction( s, buf );

	result = receive_result( s );
	if( result == 1 )
		printf( "Changed current directory\n" );
	else if( result == -1 )
		printf( "Error in changing directory\n" );
	else if( result == -2 )
		printf( "The directory does not exist on server\n" );
}

void send_instruction(int s, char* message) {
	long len, sendlen;
	int i;

	len = strlen( message ) + 1;
	len = htonl( len );

	// send the length of the message
	if( write( s, &len, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrm: error sending size\n" );
		return;
	}

	len = ntohl( len );

	// send the actual message
	for (i = 0; i < len; i += MAX_LINE) {
		sendlen = (len-i < MAX_LINE) ? len-i : MAX_LINE;
		if( write( s, &message[i], sendlen ) == -1 ){
			fprintf( stderr, "myfrm: error sending message\n" );
			exit( 1 );
		}
	}
}

void send_name(int s) {
	char buf[MAX_LINE];

	fflush( stdin );

	if ( fgets( buf, MAX_LINE, stdin ) ) {
		int i;
		for(i = 0; i < MAX_LINE; i++) {
			if (buf[i] == '\n' || buf[i] == ' ') {
				buf[i] = '\0'; 
				break;
			}
		}// set null characters

		if ( send( s, buf, MAX_LINE, 0) == -1 ) {
			fprintf( stderr, "myfrm: error sending name\n");
			exit(1);
		}
	} else {
		fprintf( stderr, "myfrm: error, no name received \n");
		exit(1);
	}
}

uint16_t receive_result(int s){
	uint16_t result;

	if( read( s, &result, sizeof(uint16_t) ) == -1 ){
		fprintf( stderr, "myfrm: error receiving result\n" );
		return 0;
	}

	return ntohs( result );
}

long receive_result32(int s){
	long result;

	if( read( s, &result, sizeof(long) ) == -1 ){
		fprintf( stderr, "myfrm: error receiving result\n" );
		return 0;
	}

	return ntohl( result );
}
