#ifndef SERVER_UTIL_H
#define SERVER_UTIL_H

#define TIME_OUT_EXPIRED -2

void send_to_client(int socket, char* buf);

size_t recv_from_client(int socket, char* buf, size_t buf_len);

bool_t name_validation(char* name, size_t len);

void server_intial_setup(int socket_desc);

#endif