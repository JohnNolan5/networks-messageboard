// myfrm.h

#ifndef MYFRM_H
#define MYFRM_H

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <mhash.h>
#include <sys/stat.h> // for directory status

void handle_action( char*, int, int);

void create_board(int);
void leave_message(int);
void delete_message(int);
void edit_message(int);
void list_boards(int);
void read_board(int);
void append_file(int);
void download_file(int);
void destroy_board(int);

void send_name(int);
void send_instruction( int, char* );
uint16_t receive_result( int );
long receive_result32(int); // changed from uint32_t
void send_file( int, FILE*, long);

#endif
