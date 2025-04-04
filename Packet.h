#pragma once
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

struct PACKET_HEADER
{
	uint16_t PacketLength;
	uint16_t PacketId;
};


//  ---------------------------- SYSTEM  ----------------------------

struct IM_MATCHING_REQUEST : PACKET_HEADER {

};

struct IM_MATCHING_RESPONSE : PACKET_HEADER {
	bool isSuccess;
};


//  ---------------------------- RAID  ----------------------------

struct RAID_MATCHING_REQUEST_TO_MATCHING_SERVER : PACKET_HEADER {
	uint16_t userPk;
	uint16_t userGroupNum;
};

struct RAID_MATCHING_RESPONSE_TO_CENTER_SERVER : PACKET_HEADER {
	uint16_t roomNum;
	uint16_t userNum1;
	uint16_t userNum2;
};

struct RAID_MATCHING_FAIL_RESPONSE : PACKET_HEADER { // MATCHING SERVER TO CENTER SERVER
	uint16_t userPk;
};

enum class PACKET_ID : uint16_t {
	//SYSTEM
	IM_MATCHING_REQUEST = 8, // 유저는 1번으로 요청
	IM_MATCHING_RESPONSE = 9, // 유저는 1번으로 요청
};