#pragma once
#define WIN32_LEAN_AND_MEAN 

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <cstdint>

const uint32_t SERVER_COUNT = 2;
const uint32_t MAX_RECV_SIZE = 1024; // Set Max Recv Buf
const uint32_t MAX_CIRCLE_SIZE = 8096;

enum class TaskType {
	ACCEPT = 0,
	RECV = 1,
	SEND = 2,
	NEWRECV, // ������ Ǯ �� �Ἥ ���� ���� ����Ѱ�. (�̰� �� ���� ����)
	NEWSEND
};

struct OverlappedEx {
	WSAOVERLAPPED wsaOverlapped;
	// 16 bytes
	WSABUF wsaBuf; // TCP Buffer
	// 4 bytes
	TaskType taskType; // ACCPET, RECV, SEND, LASTSEND
	// 2 bytes
	uint16_t connObjNum;
};
