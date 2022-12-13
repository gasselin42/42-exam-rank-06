#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_client
{
	int	fd;
	int	id;
	struct s_client *next;
}	t_client;

int sockfd, g_id = 0;
fd_set active_sockets, read_fd, write_fd;
char read_buf[42*4096], cpy_buf[42*4096], write_buf[42*4096+42];
char msg[50];
t_client * client = NULL;

void print_error(char *msg) {
	write(2, msg, strlen(msg));
	exit(1);
}

int get_max_fd() {
	int max = sockfd;

	for (t_client * tmp = client; tmp; tmp = tmp->next) {
		if (tmp->fd > max)
			max = tmp->fd;
	}
	return (max);
}

int add_to_list(int fd) {
	t_client * tmp = client;
	t_client * new = NULL;

	if (!(new = calloc(1, sizeof(t_client))))
		print_error("Fatal error\n");

	new->fd = fd;
	new->id = g_id++;
	new->next = NULL;

	if (!client)
		client = new;
	else {
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = new;
	}
	return new->id;
}

void send_all(int sender_fd, char * message) {
	for (t_client * tmp = client; tmp; tmp = tmp->next) {
		if (FD_ISSET(tmp->fd, &write_fd) && tmp->fd != sender_fd)
			if (send(tmp->fd, message, strlen(message), 0) < 0)
				print_error("Fatal error\n");
	}
}

int get_client_id(int fd) {
	for (t_client * tmp = client; tmp; tmp = tmp->next) {
		if (tmp->fd == fd)
			return tmp->id;
	}
	return -1;
}

void accept_connection() {
	struct sockaddr_in cli;
	socklen_t len = sizeof(cli);
	int fd;

	if ((fd = accept(sockfd, (struct sockaddr *)&cli, &len)) < 0)
		print_error("Fatal error\n");

	bzero(&msg, sizeof(msg));
	sprintf(msg, "server: client %d just arrived\n", add_to_list(fd));
	send_all(sockfd, msg);
	FD_SET(fd, &active_sockets);
}

void disconnect_client(int fd) {
	t_client * tmp = client;
	t_client * to_del = NULL;

	int cliend_id = get_client_id(fd);

	if (tmp && tmp->fd == fd) {
		client = tmp->next;
		free(tmp);
	}
	else {
		while (tmp && tmp->next && tmp->next->fd != fd)
			tmp = tmp->next;
		to_del = tmp->next;
		tmp->next = tmp->next->next;
		free(to_del);
	}

	bzero(&msg, sizeof(msg));
	sprintf(msg, "server: client %d just left\n", cliend_id);
	send_all(sockfd, msg);
	FD_CLR(fd, &active_sockets);
	close(fd);
}

void send_message(int fd) {
	int i = 0, j = 0;

	while (read_buf[i]) {
		cpy_buf[j] = read_buf[i];
		j++;
		if (read_buf[i] == '\n') {
			bzero(&write_buf, sizeof(write_buf));
			sprintf(write_buf, "client %d: %s", get_client_id(fd), cpy_buf);
			send_all(fd, write_buf);
			bzero(&write_buf, sizeof(write_buf));
			bzero(&cpy_buf, sizeof(cpy_buf));
			j = 0;
		}
		i++;
	}
	bzero(&read_buf, sizeof(read_buf));
}

void send_or_disconnect_client() {
	for (int fd = 3; fd <= get_max_fd(); fd++) {
		if (FD_ISSET(fd, &read_fd)) {
			ssize_t count = recv(fd, &read_buf, sizeof(read_buf), 0);
			if (count <= 0) {
				disconnect_client(fd);
				break;
			}
			else {
				send_message(fd);
			}
		}
	}
}

int main(int ac, char ** av) {

	if (ac < 2)
		print_error("Wrong number of arguments\n");

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		print_error("Fatal error\n");
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		print_error("Fatal error\n");
	if (listen(sockfd, 10) != 0)
		print_error("Fatal error\n");

	FD_ZERO(&active_sockets);
	FD_SET(sockfd, &active_sockets);
	bzero(&read_buf, sizeof(read_buf));
	bzero(&cpy_buf, sizeof(cpy_buf));
	bzero(&write_buf, sizeof(write_buf));
	bzero(&msg, sizeof(msg));

	while (42) {
		read_fd = write_fd = active_sockets;
		if (select(get_max_fd() + 1, &read_fd, &write_fd, NULL, NULL) < 0)
			continue ;
		if (FD_ISSET(sockfd, &read_fd)) {
			accept_connection();
			continue;
		}
		else {
			send_or_disconnect_client();
		}
	}
}