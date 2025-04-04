#include "MatchingManager.h"

bool MatchingManager::Init(const uint16_t maxClientCount_) {
    WSAData wsadata;
    int check = 0;

    check = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (check) {
        std::cout << "WSAStartup() Fail" << std::endl;
        return false;
    }

    serverIOSkt = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
    if (serverIOSkt == INVALID_SOCKET) {
        std::cout << "Server Socket Make Fail" << std::endl;
        return false;
    }

    IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);

    if (IOCPHandle == NULL) {
        std::cout << "Iocp Handle Make Fail" << std::endl;
        return false;
    }

    auto bIOCPHandle = CreateIoCompletionPort((HANDLE)serverIOSkt, IOCPHandle, (ULONG_PTR)this, 0);
    if (bIOCPHandle == nullptr) {
        std::cout << "Iocp Handle Bind Fail" << std::endl;
        return false;
    }

    SOCKADDR_IN addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    std::cout << "Connect To Center Server" << std::endl;

    connect(serverIOSkt, (SOCKADDR*)&addr, sizeof(addr));

    std::cout << "Connect Center Server Success" << std::endl;

    IM_MATCHING_REQUEST imReq;
    imReq.PacketId = (UINT16)PACKET_ID::IM_MATCHING_REQUEST;
    imReq.PacketLength = sizeof(IM_MATCHING_REQUEST);

    send(serverIOSkt, (char*)&imReq, sizeof(imReq), 0);
    recv(serverIOSkt, recvBuf, PACKET_SIZE, 0);

    auto imRes = reinterpret_cast<IM_MATCHING_RESPONSE*>(recvBuf);

    if (!imRes->isSuccess) {
        std::cout << "Fail to Connet" << std::endl;
        return false;
    }

    overlappedManager = new OverLappedManager;

    for (int i = 1; i <= USER_MAX_LEVEL / 3 + 1; i++) { // Max i = MaxLevel/3 + 1 (Level Check Set)
        matchingMap.emplace(i, std::set<MatchingRoom*, MatchingRoomComp>());
    }

    for (int i = 1; i <= MAX_ROOM; i++) { // Room Number Set
        roomNumQueue.push(i);
    }

    CreateWorkThread();
    CreateMatchThread();
    CreatePacketThread();

    return true;
}


bool MatchingManager::RecvData() {
    OverlappedEx* tempOvLap = (overlappedManager->getOvLap());

    if (tempOvLap == nullptr) { // ������ Ǯ�� ���� ������ ���� ������ ����
        OverlappedEx* overlappedEx = new OverlappedEx;
        ZeroMemory(overlappedEx, sizeof(OverlappedEx));
        overlappedEx->wsaBuf.len = PACKET_SIZE;
        overlappedEx->wsaBuf.buf = new char[PACKET_SIZE];
        overlappedEx->taskType = TaskType::NEWSEND;
    }
    else {
        tempOvLap->wsaBuf.len = PACKET_SIZE;
        tempOvLap->wsaBuf.buf = new char[PACKET_SIZE];
        tempOvLap->taskType = TaskType::RECV;
    }

    DWORD dwFlag = 0;
    DWORD dwRecvBytes = 0;

    int tempR = WSARecv(serverIOSkt, &(tempOvLap->wsaBuf), 1, &dwRecvBytes, &dwFlag, (LPWSAOVERLAPPED)tempOvLap, NULL);

    if (tempR == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
    {
        std::cout << " WSARecv() Fail : " << WSAGetLastError() << std::endl;
        return false;
    }

    return true;
}

//
//void MatchingManager::PushPacket(const uint32_t size_, char* recvData_) {
//	procQueue.push(recvData_); // Push Packet
//}

bool MatchingManager::CreatePacketThread() {
	packetRun = true;
	packetThread = std::thread([this]() {PacketThread(); });
	std::cout << "PacketThread Start" << std::endl;
	return true;
}

void MatchingManager::PacketThread() {
    MatchingRoom* tempRoom;
    tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor;

	while (packetRun) {
		char* recvData = nullptr;
		if (procQueue.pop(recvData)) { // Packet Exist
            auto k = reinterpret_cast<RAID_MATCHING_REQUEST_TO_MATCHING_SERVER*>(recvData);

            if (!Insert(k->userPk, k->userGroupNum)) {
                // �߾� ������ ��Ī ���� ����

            }
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}
}

bool MatchingManager::Insert(uint16_t userPk_, uint16_t userGroupNum_) {
    MatchingRoom* tempRoom = new MatchingRoom(userPk_);

    tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor;
    uint16_t groupNum = userGroupNum_;

    if (matchingMap.find(accessor, groupNum)) { // Insert Success
        accessor->second.insert(tempRoom);
        std::cout << "Insert Group " << groupNum << std::endl;
        return true;
    }

    // Match Queue Full || Insert Fail
    return false;
}

bool MatchingManager::CancelMatching(uint16_t userPk_, uint16_t userGroupNum_) {
    tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor;
    uint16_t groupNum = userGroupNum_;
    matchingMap.find(accessor, groupNum);
    auto tempM = accessor->second;

    {
        std::lock_guard<std::mutex> guard(mDeleteMatch);
        for (auto iter = tempM.begin(); iter != tempM.end(); iter++) {
            if ((*iter)->userObjNum == userPk_) { // ��Ī ����� ���� ã��
                delete* iter;
                tempM.erase(iter);
                return true;
            }
        }
    }
    return false;
}

bool MatchingManager::CreateMatchThread() {
    matchRun = true;
    matchingThread = std::thread([this]() {MatchingThread(); });
    std::cout << "MatchThread Start" << std::endl;
    return true;
}

bool MatchingManager::CreateWorkThread() {
    workRun = true;
    workThread = std::thread([this]() {WorkThread(); });
    std::cout << "WorkThread Start" << std::endl;
    return true;
}

void MatchingManager::WorkThread() {
    LPOVERLAPPED lpOverlapped = NULL;
    DWORD dwIoSize = 0;
    bool gqSucces = TRUE;
    ULONG_PTR compKey = 0;

    while (workRun) {
        gqSucces = GetQueuedCompletionStatus(
            IOCPHandle,
            &dwIoSize,
            &compKey,
            &lpOverlapped,
            INFINITE
        );

        if (gqSucces && dwIoSize == 0 && lpOverlapped == NULL) { // Server End Request
            workRun = false;
            continue;
        }

        auto overlappedEx = (OverlappedEx*)lpOverlapped;

        if (overlappedEx->taskType == TaskType::RECV) {
            procQueue.push(overlappedEx->wsaBuf.buf); // Push Packet
            RecvData();
            
        }
        else if (overlappedEx->taskType == TaskType::NEWRECV) {
            procQueue.push(overlappedEx->wsaBuf.buf); // Push Packet
            RecvData();
            delete[] overlappedEx->wsaBuf.buf;
            delete overlappedEx;
        }
        else if (overlappedEx->taskType == TaskType::SEND) {

        }
        else if (overlappedEx->taskType == TaskType::NEWSEND) {

        }
    }
}

void MatchingManager::MatchingThread() {
    uint16_t cnt = 1;
    uint16_t tempRoomNum = 0;
    MatchingRoom* tempMatching1;
    MatchingRoom* tempMatching2;

    while (matchRun) {
        if (tempRoomNum == 0) { // ��� �ѹ� �̾Ƽ� ���� �̾ƾ���
            if (roomNumQueue.pop(tempRoomNum)) { // Exist Room Num
                for (int i = cnt; i <= 6; i++) {
                    tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor1;
                    if (matchingMap.find(accessor1, i)) { // i��° ���� �׷� �ѹ� üũ

                        if (!accessor1->second.empty()) { // ���� �Ѹ��̶� ����
                            tempMatching1 = *accessor1->second.begin();

                            if (tempMatching1->inGameUser->GetId() != inGameUserManager->GetInGameUserByObjNum((connUsersManager->FindUser(tempMatching1->userObjNum)
                                ->GetObjNum()))->GetId()) { // ���� �������� ������ ���̵�� ���ؼ� �ٸ��� �̹� ���� ���������� �������� �Ѿ��
                                accessor1->second.erase(accessor1->second.begin());
                                delete tempMatching1;
                                continue;
                            }

                            accessor1->second.erase(accessor1->second.begin());

                            if (!accessor1->second.empty()) { // �ι�° ��� ������ ����
                                tempMatching2 = *accessor1->second.begin();

                                if (tempMatching2->inGameUser->GetId() != inGameUserManager->GetInGameUserByObjNum((connUsersManager->FindUser(tempMatching2->userObjNum)->GetObjNum()))
                                    ->GetId()) { // �̹� ���� ������ �������� �Ѿ��
                                    accessor1->second.erase(accessor1->second.begin());
                                    delete tempMatching2;
                                    continue;
                                }

                                accessor1->second.erase(accessor1->second.begin());

                                { // �θ� ���� �� ���� �־��ֱ�
                                    RAID_READY_REQUEST rReadyResPacket1;
                                    RAID_READY_REQUEST rReadyResPacket2;

                                    // Send to User1 With User2 Info
                                    rReadyResPacket1.PacketId = (uint16_t)PACKET_ID::RAID_READY_REQUEST;
                                    rReadyResPacket1.PacketLength = sizeof(RAID_READY_REQUEST);
                                    rReadyResPacket1.timer = 2;
                                    rReadyResPacket1.roomNum = tempRoomNum;
                                    rReadyResPacket1.yourNum = 0;
                                    rReadyResPacket1.mobHp = 30; // ���߿� ���� hp Map ���� �����ϱ�

                                    // Send to User2 with User1 Info
                                    rReadyResPacket2.PacketId = (uint16_t)PACKET_ID::RAID_READY_REQUEST;
                                    rReadyResPacket2.PacketLength = sizeof(RAID_READY_REQUEST);
                                    rReadyResPacket2.timer = 2;
                                    rReadyResPacket2.roomNum = tempRoomNum;
                                    rReadyResPacket2.yourNum = 1;
                                    rReadyResPacket2.mobHp = 30; // ���߿� ���� hp Map ���� �����ϱ�

                                    // ������ ��û ó�� �ڿ� �� ���� ��û ������ (���� ��û �� �� ó���ϰ� �� ����)
                                    connUsersManager->FindUser(tempMatching1->userObjNum)->PushSendMsg(sizeof(RAID_READY_REQUEST), (char*)&rReadyResPacket1);
                                    connUsersManager->FindUser(tempMatching2->userObjNum)->PushSendMsg(sizeof(RAID_READY_REQUEST), (char*)&rReadyResPacket2);

                                    endRoomCheckSet.insert(roomManager->MakeRoom(tempRoomNum, 2, 30, tempMatching1->userObjNum,
                                        tempMatching2->userObjNum, tempMatching1->inGameUser, tempMatching2->inGameUser));
                                }

                                delete tempMatching1;
                                delete tempMatching2;

                                if (cnt == 6) cnt = 1; // ��� ��Ī�� ���� ��ȣ �׷���� ��Ī�� ���� cnt üũ
                                else cnt++;
                                tempRoomNum = 0;
                                break;
                            }
                            else { // ���� ������ ��� ���� �Ѹ��̶� �ٽ� �ֱ�
                                accessor1->second.insert(tempMatching1);
                            }
                        }
                    }
                }
            }
            else { // Not Exist Room Num
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        else { // �̾Ƶ� ��� ����
            for (int i = 1; i <= 6; i++) {
                tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor1;
                if (matchingMap.find(accessor1, i)) { // i��° ���� �׷� �ѹ� üũ

                    if (!accessor1->second.empty()) { // ���� �Ѹ��̶� ����
                        tempMatching1 = *accessor1->second.begin();

                        if (tempMatching1->inGameUser->GetId() != inGameUserManager->GetInGameUserByObjNum((connUsersManager->FindUser(tempMatching1->userObjNum)->GetObjNum()))->GetId()) { // ���� �������� ������ ���̵�� ���ؼ� �ٸ��� �̹� ���� ���������� �������� �Ѿ��
                            accessor1->second.erase(accessor1->second.begin());
                            delete tempMatching1;
                            continue;
                        }
                        accessor1->second.erase(accessor1->second.begin());

                        if (!accessor1->second.empty()) { // �ι�° ��� ������ ����
                            tempMatching2 = *accessor1->second.begin();

                            if (tempMatching2->inGameUser->GetId() != inGameUserManager->GetInGameUserByObjNum((connUsersManager->FindUser(tempMatching2->userObjNum)->GetObjNum()))->GetId()) { // �̹� ���� ������ �������� �Ѿ��
                                accessor1->second.erase(accessor1->second.begin());
                                delete tempMatching2;
                                continue;
                            }

                            accessor1->second.erase(accessor1->second.begin());

                            { // �θ� ���� �� ���� �־��ֱ�
                                RAID_READY_REQUEST rReadyResPacket1;
                                RAID_READY_REQUEST rReadyResPacket2;

                                // Send to User1 With User2 Info
                                rReadyResPacket1.PacketId = (uint16_t)PACKET_ID::RAID_READY_REQUEST;
                                rReadyResPacket1.PacketLength = sizeof(RAID_READY_REQUEST);
                                rReadyResPacket1.timer = 2;
                                rReadyResPacket1.roomNum = tempRoomNum;
                                rReadyResPacket1.yourNum = 0;
                                rReadyResPacket1.mobHp = 30; // ���߿� ���� hp Map ���� �����ϱ�

                                // Send to User2 with User1 Info
                                rReadyResPacket2.PacketId = (uint16_t)PACKET_ID::RAID_READY_REQUEST;
                                rReadyResPacket2.PacketLength = sizeof(RAID_READY_REQUEST);
                                rReadyResPacket2.timer = 2;
                                rReadyResPacket2.roomNum = tempRoomNum;
                                rReadyResPacket2.yourNum = 1;
                                rReadyResPacket2.mobHp = 30; // ���߿� ���� hp Map ���� �����ϱ�

                                // ������ ��û ó�� �ڿ� �� ���� ��û ������ (���� ��û �� �� ó���ϰ� �� ����)
                                connUsersManager->FindUser(tempMatching1->userObjNum)->PushSendMsg(sizeof(RAID_READY_REQUEST), (char*)&rReadyResPacket1);
                                connUsersManager->FindUser(tempMatching2->userObjNum)->PushSendMsg(sizeof(RAID_READY_REQUEST), (char*)&rReadyResPacket2);

                                endRoomCheckSet.insert(roomManager->MakeRoom(tempRoomNum, 2, 30, tempMatching1->userObjNum, tempMatching2->userObjNum, tempMatching1->inGameUser, tempMatching2->inGameUser));
                            }

                            delete tempMatching1;
                            delete tempMatching2;

                            if (cnt == 6) cnt = 1;
                            else cnt++;
                            tempRoomNum = 0;
                            break;
                        }
                        else { // ���� ������ ��� ���� �Ѹ��̶� �ٽ� �ֱ�
                            accessor1->second.insert(tempMatching1);
                        }
                    }
                }
            }
        }
    }
}