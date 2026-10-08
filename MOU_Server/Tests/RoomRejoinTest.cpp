#include "Rooms/Rooms.h"
#include <cstdlib>
#include <iostream>

// [REJOINTEST-001] 검증 실패를 출력하고 회귀 테스트를 중단한다.
static void Check(bool Value, const char* Label)
{
    if (!Value) { std::cerr << Label << '\n'; std::exit(1); }
}

// [REJOINTEST-002] 게임중 목록·중도 입장·재입장·만료된 승인·호스트 종료를 검증한다.
int main()
{
    using namespace MOU;
    uint32_t Room = 0, Changed = 0;
    bool Lan = false, Closed = false;
    std::vector<HostCandidate> Candidates(1);
    std::vector<uint64_t> Recipients;
    Check(Rooms::Create(10, "host", Candidates, false, "rejoin", true, "1234", 3, Room)
        == ERoomResult::Success, "create");
    Check(Rooms::Join(Room, 11, "first", "1234", Candidates, Lan) == ERoomResult::Success, "waiting join");
    Check(Rooms::SetReady(11, true, Changed) == ERoomResult::Success, "ready");
    Check(Rooms::StartGame(10, Changed, Candidates, Lan, Recipients) == ERoomResult::Success, "start");
    std::vector<RoomInfo> List;
    Rooms::ListWaiting(List, 100);
    Check(List.size() == 1 && List[0].State == static_cast<uint8_t>(ERoomState::InGame), "game missing from list");
    Rooms::JoinContext Join;
    Check(Rooms::Join(Room, 12, "late", "0000", Candidates, Lan, &Join) == ERoomResult::WrongPassword, "password bypass");
    Check(Rooms::Join(Room, 12, "late", "1234", Candidates, Lan, &Join) == ERoomResult::Success, "late join");
    Check(Join.State == ERoomState::InGame && Join.HostUserId == 10 && Join.ConnectRequestId != 0, "join context");
    const auto OldRequest = Join.ConnectRequestId;
    Check(Rooms::MarkHostReady(10, Changed, Candidates, Lan, Recipients) == ERoomResult::Success, "initial ready");
    Check(Recipients.size() == 1 && Recipients[0] == 11, "late guest received initial ready");
    Check(!Rooms::CompleteGuestConnect(11, Room, 12, OldRequest, Candidates, Lan), "nonhost approval");
    Check(Rooms::Join(Room, 13, "full", "1234", Candidates, Lan) == ERoomResult::Full, "full accepted");
    Rooms::ListWaiting(List, 100);
    Check(List.size() == 1 && List[0].CurrentPlayers == 3, "full room hidden");
    Rooms::Leave(12, Changed, Closed, Recipients);
    Check(!Closed && !Rooms::CompleteGuestConnect(10, Room, 12, OldRequest, Candidates, Lan), "departed approval");
    Check(Rooms::Join(Room, 12, "late", "1234", Candidates, Lan, &Join) == ERoomResult::Success, "same account rejoin");
    Check(Join.ConnectRequestId != OldRequest, "request id reused");
    Check(!Rooms::CompleteGuestConnect(10, Room, 12, OldRequest, Candidates, Lan), "stale approval");
    Check(Rooms::CompleteGuestConnect(10, Room, 12, Join.ConnectRequestId, Candidates, Lan), "current approval");
    Check(!Rooms::CompleteGuestConnect(10, Room, 12, Join.ConnectRequestId, Candidates, Lan), "duplicate approval");
    Rooms::Leave(11, Changed, Closed, Recipients);
    Check(Rooms::Join(Room, 13, "new", "1234", Candidates, Lan, &Join) == ERoomResult::Success, "new account midgame");
    Rooms::Leave(10, Changed, Closed, Recipients);
    Check(Closed && Rooms::Count() == 0, "host departure");
    Check(!Rooms::CompleteGuestConnect(10, Room, 13, Join.ConnectRequestId, Candidates, Lan), "closed approval");
    Check(Rooms::Create(12, "returned", Candidates, false, "new room", false, "", 4, Room)
        == ERoomResult::Success, "new room after previous closed");
    Rooms::Leave(12, Changed, Closed, Recipients);
    std::cout << "RoomRejoinTest passed\n";
}
