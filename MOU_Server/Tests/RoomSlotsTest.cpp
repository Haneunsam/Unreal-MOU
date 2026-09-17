#include "Rooms/Rooms.h"
#include <cstdlib>
#include <iostream>
#include <map>

static void Check(bool Condition, const char* Message)
{
    if (!Condition) { std::cerr << Message << '\n'; std::exit(1); }
}

int main()
{
    using namespace MOU;
    uint32_t RoomId = 0;
    std::vector<HostCandidate> Candidates(1);
    Check(Rooms::Create(100, "host", Candidates, true, "slots", false, "", 4, RoomId)
        == ERoomResult::Success, "create failed");
    bool Lan = false;
    auto Join = [&](uint64_t Id) { return Rooms::Join(RoomId, Id, "guest", "", Candidates, Lan); };
    auto Verify = [&](const std::map<uint64_t, int>& Expected)
    {
        std::vector<RoomMemberInfo> Members;
        std::vector<uint64_t> Notify;
        bool AllReady = false;
        Check(Rooms::GetMembers(RoomId, Members, AllReady, Notify), "missing room");
        Check(Members.size() == Expected.size(), "wrong member count");
        for (const auto& Member : Members)
        {
            auto It = Expected.find(Member.UserId);
            Check(It != Expected.end() && It->second == Member.SlotIndex, "seat changed");
        }
    };
    Check(Join(101) == ERoomResult::Success, "join 101");
    Check(Join(102) == ERoomResult::Success, "join 102");
    Check(Join(103) == ERoomResult::Success, "join 103");
    Verify({{100,0},{101,1},{102,2},{103,3}});
    Check(Join(104) != ERoomResult::Success, "full room accepted member");
    uint32_t ChangedRoom = 0;
    bool Closed = false;
    std::vector<uint64_t> Notify;
    Rooms::Leave(101, ChangedRoom, Closed, Notify);
    Check(!Closed && ChangedRoom == RoomId, "guest closed room");
    Verify({{100,0},{102,2},{103,3}});
    Check(Rooms::SetReady(102, true, ChangedRoom) == ERoomResult::Success, "ready failed");
    Verify({{100,0},{102,2},{103,3}});
    Join(102); // Repeated request must not allocate a second seat.
    Verify({{100,0},{102,2},{103,3}});
    Check(Join(104) == ERoomResult::Success, "refill failed");
    Verify({{100,0},{104,1},{102,2},{103,3}});
    Rooms::Leave(100, ChangedRoom, Closed, Notify);
    Check(Closed && Rooms::Count() == 0, "host leave did not close room");
    std::cout << "RoomSlotsTest passed\n";
}
