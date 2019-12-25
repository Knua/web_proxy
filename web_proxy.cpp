#include <stdio.h> // for perror
#include <stdlib.h> // for atoi
#include <string.h> // for memset
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket
#include <netdb.h> // for hostname

#include <thread> // for thread
#include <algorithm> // for find
#define BUF_SIZE 4096
#define HOSTNAME_SIZE 100

using namespace std;

void usage() {
    printf("syntax: web_proxy <tcp port>\n");
    printf("sample: web_proxy 8080\n");
}

void relay(int recvfd, int sendfd){
	char buf[BUF_SIZE];

	while (true) {
		ssize_t received = recv(recvfd, buf, BUF_SIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			break;
		}
		buf[received] = '\0';
		printf("Received Msg from %d\n%s\n", recvfd, buf);

		if (send(sendfd, buf, strlen(buf), 0) == 0) {
			perror("send failed");
			break;
		}
		else {}
	}
	close(recvfd);
	close(sendfd);
}

void hostname_find(char * data, char * hostname){

	if(strncmp((const char *)(data), "GET", 3) == 0 ||
	strncmp((const char *)(data), "POST", 4) == 0 ||
	strncmp((const char *)(data), "HEAD", 4) == 0 ||
	strncmp((const char *)(data), "PUT", 3) == 0 ||
	strncmp((const char *)(data), "DELETE", 6) == 0 ||
	strncmp((const char *)(data), "OPTIONS", 7) == 0) {
		// \x0d \x0a -> end of string
		const char * ptr = strstr((const char *)(data), "Host:");
		if(ptr != NULL){
			char save_string[HOSTNAME_SIZE] = "";
			for(int i = 6;; i++){
				if(strncmp((ptr + i), "\x0d", 1) == 0) break;
				strncat(save_string, (ptr + i), 1);
			}
			strncpy(hostname, save_string, HOSTNAME_SIZE);
			return;
		}
		else printf("[Work] No HostName..\n");
	}
	strncpy(hostname, "", 1);
	return;
}

int main(int argc, char  * argv[]) 
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}
	if (argc != 2){
        usage();
        return -1;
    }
	printf("Proxy Server Working...\n");
    int port = atoi(argv[1]);
	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2); // backlog
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

	while(true) {
		struct sockaddr_in new_addr;
		socklen_t clientlen = sizeof(sockaddr);
		
		int client_fd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&new_addr), &clientlen);
		if (client_fd < 0) {
			perror("ERROR on accept");
			close(client_fd);
			return -1;
		}
		printf("\nClient-Proxy connected\n\n");

			// First. We must get hostname
		char buf[BUF_SIZE];
		ssize_t received = recv(client_fd, buf, BUF_SIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			close(client_fd);
			break;
		}
		buf[received] = '\0';
		printf("[Work] Received Msg from %d\n", client_fd);
		printf("%s\n", buf);

		char hostname[HOSTNAME_SIZE];
		hostname_find(buf, hostname);
		if(hostname == ""){
			close(client_fd);
			continue;
		}
		struct hostent* host_info;
		host_info = gethostbyname(hostname);

			// Second. Connect Web-server(hostname)
		int webserver_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (webserver_fd == -1) {
			perror("socket failed");
			close(client_fd); 
			close(webserver_fd);
			continue;
		}

		struct sockaddr_in now_addr;
		now_addr.sin_family = AF_INET;
		now_addr.sin_port = htons(80); // http
		now_addr.sin_addr.s_addr = *(unsigned int *)host_info->h_addr;
		memset(now_addr.sin_zero, 0, sizeof(now_addr.sin_zero));

		res = connect(webserver_fd, reinterpret_cast<struct sockaddr*>(&now_addr), sizeof(struct sockaddr));
		if (res == -1) {
			perror("bind failed");
			close(client_fd);
			close(webserver_fd);
			continue;
		}
		printf("\nProxy-Webserver connected\n\n");

		ssize_t sent = send(webserver_fd, buf, strlen(buf), 0);
		if (sent == 0) {
			perror("send failed");
			close(client_fd);
			close(webserver_fd);
			continue;
		}
		thread(relay, client_fd, webserver_fd).detach();
		thread(relay, webserver_fd, client_fd).detach();
	}
	close(sockfd);
}