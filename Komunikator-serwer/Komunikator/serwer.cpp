#include <cstdlib>
#include <cstdio>
#include <conio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <windows.h>
#include <queue>

struct Client
{
	SOCKET sock;
	sockaddr address;
	bool voice;
	int sock_size;
};

void Cleanup(); //sleep(7000)
int closeBrokerSocket(SOCKET sock);
int readStatus(const SOCKET sock);

int main()
{
	WSADATA wsaData;

	int result; //value to store functions results
	
	if ((result = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		printf("WSAStartup() error: %ld\n", result);
		return EXIT_FAILURE;
	}

	sockaddr_in myAddr;
	memset(&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	InetPton(AF_INET, "127.0.0.1", (void *)&myAddr.sin_addr);
	myAddr.sin_port = htons(27015);

	atexit(Cleanup);

	SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSock == INVALID_SOCKET)
	{
		printf("socket() error: %ld", WSAGetLastError());
		return EXIT_FAILURE;
	}

	unsigned long argp = 1; // non-blocking sockets

	result = setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&argp, sizeof(argp));
	if (result != 0)
	{
		printf("setsockopt() error: %ld", result);
		return EXIT_FAILURE;
	}

	if (ioctlsocket(serverSock, FIONBIO, &argp) == SOCKET_ERROR)
	{
		printf("ioctlsocket() error: %ld", WSAGetLastError());
		return EXIT_FAILURE;
	}

	if (bind(serverSock, (SOCKADDR *)&myAddr, sizeof(myAddr)) == SOCKET_ERROR)
	{
		printf("Failed to bind server socket!\n");
		closesocket(serverSock);
		getchar();
		return -1;
	}


	if (listen(serverSock, 10) == SOCKET_ERROR)
		printf("listen() error: %ld\n", WSAGetLastError());

	printf("Server is now running!\n");

	std::vector<Client> clients; //Socket, voice_power
	std::vector<char> inputBuffer;

	while ((result = readStatus(serverSock)) != SOCKET_ERROR)
	{
		if (_kbhit() && _getch() == 27) // escape key pressed
		{
			break;
		}

		if (result == 1)
		{
			printf("Trying to connect a client...\n");
			
			Client cSock;
			cSock.sock_size = sizeof(sockaddr_in);
			cSock.sock = accept(serverSock, (struct sockaddr *)&cSock.address, &cSock.sock_size);
			
			if (cSock.sock == INVALID_SOCKET)
			{
			printf("accept() error: %ld\n", WSAGetLastError());
			return EXIT_FAILURE;
			}

			clients.push_back(cSock);

			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &((struct sockaddr_in *)&cSock.address)->sin_addr, ip, INET_ADDRSTRLEN);
			printf("Client connected. IP: %s\n", ip);
		}
		else
		{
			inputBuffer.clear();
			for (unsigned int i = 0; i < clients.size(); i++)
			{
				int status = readStatus(clients[i].sock);

				if (status == 1)
				{
					if (clients[i].sock == INVALID_SOCKET)
						continue;

					unsigned long remaining = 0;

					ioctlsocket(clients[i].sock, FIONREAD, &remaining);
					//remaining--; //input from user is always ended with '\0'

					if (remaining > 0)
					{
						unsigned long used = inputBuffer.size();
						inputBuffer.resize(used + remaining, 0);
						result = recv(clients[i].sock, (char *)(&inputBuffer[used]), remaining, 0);

						if (result == SOCKET_ERROR)
						{
							char ip[INET_ADDRSTRLEN];
							inet_ntop(AF_INET, &((struct sockaddr_in *)&clients[i].address)->sin_addr, ip, INET_ADDRSTRLEN);
							printf("ClientIP: %s recv() error: %ld\n", ip, WSAGetLastError());

							return EXIT_FAILURE;
						}
					}

					if (inputBuffer.size() > 0)
					{
						printf("Message: \"");
						for (auto v : inputBuffer)
						{
							printf("%c", v);
						}

						printf("\" %d bytes\n", inputBuffer.size());

						for (unsigned int j = 0; j < clients.size(); j++)
						{
							if (clients[j].sock == INVALID_SOCKET || clients[i].sock == clients[j].sock)
								continue;

							result = send(clients[j].sock, (const char *)&inputBuffer[0], inputBuffer.size(), 0);

							if (result == SOCKET_ERROR)
							{
								char ip[INET_ADDRSTRLEN];
								inet_ntop(AF_INET, &((struct sockaddr_in *)&clients[j].address)->sin_addr, ip, INET_ADDRSTRLEN);
								printf("ClientIP: %s send() error: %ld\n", ip, WSAGetLastError());

								result = closeBrokerSocket(clients[j].sock);

								if (result == EXIT_FAILURE)
								{
									return EXIT_FAILURE;
								}

								clients[j].sock = INVALID_SOCKET;
							}
						}
					}

				}
				else if (status == SOCKET_ERROR)
				{
					char ip[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &((struct sockaddr_in *)&clients[i].address)->sin_addr, ip, INET_ADDRSTRLEN);
					printf("ClientIP: %s read error!\n", ip);

					result = closeBrokerSocket(clients[i].sock);
					if (result == EXIT_FAILURE)
					{
						return EXIT_FAILURE;
					}

					clients[i].sock = INVALID_SOCKET;
				}
			}
		}

		if (clients.size() > 0)
		{
			for (unsigned i = clients.size() - 1; i > 0; i--)
			{
				if (clients[i].sock == INVALID_SOCKET)
				{
					clients.erase(clients.begin() + i);
				}
			}
		}
	}

	closesocket(serverSock);
	for (auto v : clients)
	{
		result = closeBrokerSocket(v.sock);

		if (result == SOCKET_ERROR)
		{
			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &((struct sockaddr_in *)&v.address)->sin_addr, ip, INET_ADDRSTRLEN);
			printf("ClientIP: %s closesocket error: %ld\n", ip, WSAGetLastError());

			result = closeBrokerSocket(v.sock);

			if (result == EXIT_FAILURE)
			{
				return EXIT_FAILURE;
			}

			v.sock = INVALID_SOCKET;
		}
	}

	_getch();

	return EXIT_SUCCESS;
}

void Cleanup()
{
	int result = WSACleanup();
	if (result != 0)
		printf("WSACleanup() error: %ld\n", result);
	Sleep(7000);
}

int closeBrokerSocket(SOCKET sock)
{
	int result = shutdown(sock, SD_BOTH);
	if (result != 0)
		printf("Socket shutdown() error %ld\n", WSAGetLastError());

	result = closesocket(sock);

	if (result != 0)
	{
		printf("closesocket() error: %ld\n", result);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int readStatus(const SOCKET sock)
{
	static const timeval speed = { 0, 0 };
	fd_set a = { 1, {sock} };

	int result = select(0, &a, 0, 0, &speed);
	if (result == SOCKET_ERROR)
	{
		result = WSAGetLastError();
	}
	if (result != 0 && result != 1)
	{
		printf("select() error: %ld\n", result);
		return SOCKET_ERROR;
	}

	return result;
}