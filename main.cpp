#include <iostream>
#include <functional>
#include <vector>
#include <thread>
#include <cerrno>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h> // for memset
#include <pwd.h> // getpwnam
#include <syslog.h> // syslog!

#include "ChatManager.hpp"
#include "ClientHandler.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::thread;

using ChatServer::ClientHandler;
using ChatServer::ChatManager;

const char* DROP_TO_USER = "chatserv";
const char* DROP_TO_GROUP = "chatgroup";

void start_processing(int fd, ChatManager& cm)
{
	ClientHandler ch(fd, cm);
	ch.HandleClient();
}

uid_t get_uid_by_name(const char *name)
{
	if(name) {
		struct passwd *pwd = getpwnam(name); /* don't free, see getpwnam() for details */
		if(pwd) return pwd->pw_uid;
	}
	return -1;
}

gid_t get_gid_by_name(const char *name)
{
	if(name) {
		struct passwd *pwd = getpwnam(name); /* don't free, see getpwnam() for details */
		if(pwd) return pwd->pw_gid;
	}
	return -1;
}

void bail(const char* msg)
{
	// Log the error
	syslog(LOG_ALERT, "Bailing: %s", msg);
	syslog(LOG_ALERT, "Errno: %s", strerror(errno));

	// Close out the logs
	closelog();

	// Exit with EXIT_FAILURE
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
	int server_sock_fd, client_sock_fd, port_number;
	socklen_t client_length;
	struct sockaddr_in server_address, client_address;
	int result = 0;
	int pid = 0;

	// Open syslog, only log LOG_NOTICE and above
	setlogmask(LOG_UPTO (LOG_NOTICE));
	openlog(argv[0], LOG_CONS | LOG_PID, 0);

	// We don't want to run as root - that's bad!
	if(getuid() == 0)
	{
		// Try to switch to the chatserv user
		uid_t newUid = get_uid_by_name(DROP_TO_USER);
		gid_t newGid = get_gid_by_name(DROP_TO_USER);
		if(newUid <= 0 || newGid <= 0)
		{
			bail("Unable to start server, could not drop privileges");
		}

		// Drop to group first
		if(setgid(newGid) != 0)
		{
			bail("Unable to drop root group");
		}

		// Drop to user
		if(setuid(newUid) != 0)
		{
			bail("Unable to drop root user");
		}

		// We really should be NOT root by now -- double check that
		if(getuid() == 0)
		{
			bail("Could not drop privileges, bailing!"); 
		}
	}
	

	// Create the ChatManager object
	ChatManager cm;

	// Vector of threads for handling clients
	vector<thread> threads;

	// Vector of client handlers
	vector<ClientHandler> handlers;

	// Create a socket
	server_sock_fd = socket(PF_INET, SOCK_STREAM, 0);

	if(server_sock_fd < 0)
	{
		bail("Error: could not create socket!"); 
	}

	// Initialize address structure
	memset((char*)&server_address, '\0', sizeof(server_address));
	port_number = 4919; // 0x1337
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(port_number);

	// Make sure the port is not in use.
	int yes = 1;
	result = setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if(result != 0)
	{
		bail("Error: could not set socket options");
	}

	// Bind
	result = bind(server_sock_fd, (struct sockaddr*)&server_address, sizeof(server_address));
	if(result != 0)
	{
		bail("Error: could not bind socket");
	}

	// Listen for connections
	result = listen(server_sock_fd, 10);
	if(result != 0)
	{
		close(server_sock_fd);
		bail("Error: could not listen on socket");
	}

	syslog(LOG_NOTICE, "Server established, listening on port %d", port_number);
	try
	{
		while(true)
		{
			// Accept the connection, spawn a new thread to handle it
			client_length = sizeof(client_address);
			client_sock_fd = accept(server_sock_fd, (struct sockaddr*)&client_address, &client_length);

			if(client_sock_fd < 0)
			{
				cerr << "Error: could not accept client (errno: " << errno << ")" << endl;
				close(client_sock_fd);
				continue;
			}

			thread t(start_processing, client_sock_fd, std::ref(cm));
			t.detach();
		}
	} 
	catch(const std::runtime_error& e)
	{
		close(server_sock_fd);
		bail("Runtime Error while accepting connections");
	}
	catch(...)
	{
		close(server_sock_fd);
		bail("GREMLINS DETECTED");
	}

	// Clean up the socket
	close(server_sock_fd);

	return 0;
}