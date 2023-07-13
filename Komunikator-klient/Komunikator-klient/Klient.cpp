#include <cstdlib>
#include <cstdio>
#include <conio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

void Cleanup();
int readStatus(const SOCKET sock);

int main()
{
	WSADATA wsaData;
	std::string clientName;

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

	printf("Connect to server as ");
	std::cin.clear();
	std::cin.sync();
	std::getline(std::cin, clientName);

	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, (struct sockaddr_in *)&myAddr.sin_addr, ip, INET_ADDRSTRLEN);
	printf("Trying to connect to the server [%s] as %s.\n", ip, clientName.c_str());

	result = connect(serverSock, (struct sockaddr *)&myAddr, sizeof(sockaddr_in));
	if (result == SOCKET_ERROR)
	{
		result = WSAGetLastError();
		switch (result)
		{
		case WSAEWOULDBLOCK:
			break;
		default:
			printf("connect() error: %ld\n", result);
			return EXIT_FAILURE;
		}
			
	}

	std::string inputBuffer;
	std::string outputBuffer;
	int ch;
	int status;
	bool sendRdy = false;

	printf("Connected.\nWrite a message:\n");

	while ((status = readStatus(serverSock)) != SOCKET_ERROR)
	{
		if (status == 1)
		{
			unsigned long remaining = 0;

			ioctlsocket(serverSock, FIONREAD, &remaining);
			//remaining--; //input from user is always ended with '\0'

			if (remaining > 0)
			{
				unsigned long used = inputBuffer.size();

				inputBuffer.resize(used + remaining, 0);
				result = recv(serverSock, (char *)(&inputBuffer[used]), remaining, 0);

				if (result == SOCKET_ERROR)
				{
					char ip[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, (struct sockaddr_in *)&myAddr.sin_addr, ip, INET_ADDRSTRLEN);
					printf("ClientIP: %s recv() error: %ld\n", ip, WSAGetLastError());

					return EXIT_FAILURE;
				}

				printf("%s\n", inputBuffer.c_str());
				inputBuffer.clear();
			}
		}
		else
		{
			if (_kbhit())
			{
				ch = _getch();
				if (ch == '\n' || ch == '\r')
					sendRdy = true;
				else
				{
					outputBuffer += ch;
					printf("%c", ch);
				}
			}

			if (sendRdy)
			{
				printf("\n");
				outputBuffer.insert(0, clientName + ": ");
				outputBuffer += '\0';

				result = send(serverSock, outputBuffer.c_str(), outputBuffer.size(), 0);

				if (result == SOCKET_ERROR)
				{
					printf("send() error: %ld", WSAGetLastError());
					return EXIT_FAILURE;
				}

				sendRdy = false;
				outputBuffer.clear();
			}
		}
	}
}


void Cleanup()
{
	int result = WSACleanup();
	if (result != 0)
		printf("WSACleanup() error: %ld\n", result);
	Sleep(7000);
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