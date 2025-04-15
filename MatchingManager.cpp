#include "MatchingManager.h"

bool MatchingManager::Init(ConnServersManager* connServersManager_) {
    connServersManager = connServersManager_;

    for (int i = 1; i <= USER_LEVEL_GROUPS; i++) {
        matchingMap.emplace(i, std::set<MatchingRoom*, MatchingRoomComp>());
    }

    for (int i = 1; i <= MAX_ROOM; i++) { // Set room number
        roomNumQueue.push(i);
    }

    CreateMatchThread();

    return true;
}

bool MatchingManager::CreateMatchThread() {
    matchRun = true;
    try {
        matchingThread = std::thread([this]() { MatchingThread(); });
    }
    catch (const std::system_error& e) {
        std::cerr << "Failed to Create Matching Threads : " << e.what() << std::endl;
        return false;
    }
    return true;
}

uint16_t MatchingManager::Insert(uint16_t userPk_, uint16_t userCenterObjNum_, uint16_t userGroupNum_) {
    MatchingRoom* tempRoom = new MatchingRoom(userPk_, userCenterObjNum_);

    tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor;
    uint16_t groupNum = userGroupNum_;

    if (matchingMap.find(accessor, groupNum)) { // Insert Success
        accessor->second.insert(tempRoom);
        std::cout << "Insert Group " << groupNum << std::endl;
        return userCenterObjNum_;
    }

    std::cout << "Fail to insert pk : " << userPk_ << std::endl;
    return 0;
}

uint16_t MatchingManager::CancelMatching(uint16_t userCenterObjNum_, uint16_t userGroupNum_) {
    tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor;
    uint16_t groupNum = userGroupNum_;
    matchingMap.find(accessor, groupNum);
    auto tempM = accessor->second;

    {
        std::lock_guard<std::mutex> guard(mDeleteMatch);
        for (auto iter = tempM.begin(); iter != tempM.end(); iter++) {
            if ((*iter)->userCenterObjNum == userCenterObjNum_) { // Find users who canceled matching
                delete* iter;
                tempM.erase(iter);
                return true;
            }
        }
    }

    return false;
}

void MatchingManager::MatchingThread() {
    uint16_t cnt = 1;
    uint16_t tempRoomNum = 0;
    MatchingRoom* tempMatching1;
    MatchingRoom* tempMatching2;

    while (matchRun) {
        if (tempRoomNum == 0) { // Need to select a new Room Number
            if (roomNumQueue.pop(tempRoomNum)) { // Select a new Room Number
                for (int i = cnt; i <= USER_LEVEL_GROUPS; i++) {
                    tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor1;
                    if (matchingMap.find(accessor1, i)) { // Check the level group number i

                        if (!accessor1->second.empty()) {
                            tempMatching1 = *accessor1->second.begin();

                            accessor1->second.erase(accessor1->second.begin());

                            if (!accessor1->second.empty()) { // Enough users available to start matchmaking

                                tempMatching2 = *accessor1->second.begin();
                                accessor1->second.erase(accessor1->second.begin());

                                { 	// Room creation on match success
                                    MATCHING_REQUEST_TO_GAME_SERVER rMatchingResPacket;

                                    // Send to User1 With User2 Info
                                    rMatchingResPacket.PacketId = (uint16_t)PACKET_ID::MATCHING_REQUEST_TO_GAME_SERVER;
                                    rMatchingResPacket.PacketLength = sizeof(MATCHING_REQUEST_TO_GAME_SERVER);
                                    rMatchingResPacket.roomNum = tempRoomNum;
									rMatchingResPacket.userCenterObjNum1 = tempMatching1->userCenterObjNum;
									rMatchingResPacket.userCenterObjNum2 = tempMatching2->userCenterObjNum;
                                    rMatchingResPacket.userPk1 = tempMatching1->userPk;
                                    rMatchingResPacket.userPk2 = tempMatching1->userPk;

                                    std::cout << "Matched Success" << std::endl;

									connServersManager->GetGameServerObjNum(1)->  // Send matched user data to the game server
                                        PushSendMsg(sizeof(MATCHING_REQUEST_TO_GAME_SERVER), (char*)&rMatchingResPacket);
                                }

                                delete tempMatching1;
                                delete tempMatching2;

                                if (cnt == 6) cnt = 1; // Continue matchmaking check from the next group
                                else cnt++;
                                tempRoomNum = 0;
                                break;
                            }
                            else { // Not enough users in the current group to start matchmaking
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
        else { // Unused RoomNum exists
            for (int i = 1; i <= USER_LEVEL_GROUPS; i++) {
                tbb::concurrent_hash_map<uint16_t, std::set<MatchingRoom*, MatchingRoomComp>>::accessor accessor1;
                if (matchingMap.find(accessor1, i)) { // Check the level group number i

                    if (!accessor1->second.empty()) { 
                        tempMatching1 = *accessor1->second.begin();

                        accessor1->second.erase(accessor1->second.begin());

                        if (!accessor1->second.empty()) { // Enough users available to start matchmaking
                            tempMatching2 = *accessor1->second.begin();
                            accessor1->second.erase(accessor1->second.begin());

                            { 	// Room creation on match success

                                MATCHING_REQUEST_TO_GAME_SERVER rMatchingResPacket;

                                // Send to User1 With User2 Info
                                rMatchingResPacket.PacketId = (uint16_t)PACKET_ID::MATCHING_REQUEST_TO_GAME_SERVER;
                                rMatchingResPacket.PacketLength = sizeof(MATCHING_REQUEST_TO_GAME_SERVER);
                                rMatchingResPacket.roomNum = tempRoomNum;
                                rMatchingResPacket.userCenterObjNum1 = tempMatching1->userCenterObjNum;
                                rMatchingResPacket.userCenterObjNum2 = tempMatching2->userCenterObjNum;
                                rMatchingResPacket.userPk1 = tempMatching1->userPk;
                                rMatchingResPacket.userPk2 = tempMatching1->userPk;

                                std::cout << "Matched Success" << std::endl;

                                connServersManager->GetGameServerObjNum(1)->  // Send matched user data to the game server
                                    PushSendMsg(sizeof(MATCHING_REQUEST_TO_GAME_SERVER), (char*)&rMatchingResPacket);

                            }

                            delete tempMatching1;
                            delete tempMatching2;

                            if (cnt == 6) cnt = 1; // Continue matchmaking check from the next group
                            else cnt++;
                            tempRoomNum = 0;
                            break;
                        }
                        else { // Not enough users in the current group to start matchmaking
                            accessor1->second.insert(tempMatching1);
                        }
                    }
                }
            }
        }
    }
}