#include "Accounts/Accounts.h"
#include <cstdio>
#include <string>
// [AUTHUI-021] 격리된 메모리 DB로 조회의 무변경성·대소문자 중복·최종 가입 충돌을 검증한다.
int main()
{
    using namespace MOU;
    int Failed = 0;
    auto Check = [&](bool Value, const char* Label) { if (!Value) { ++Failed; std::printf("FAIL: %s\n", Label); } };
    Check(Accounts::CheckLoginId("fresh") == EAccountResult::DbError, "DB failure is not available");
    Check(Accounts::Start(":memory:"), "open isolated DB");
    Check(Accounts::CheckLoginId("ab") == EAccountResult::InvalidFormat, "short ID");
    Check(Accounts::CheckLoginId(std::string(24, 'a')) == EAccountResult::InvalidFormat, "long ID");
    Check(Accounts::CheckLoginId("fresh") == EAccountResult::Success, "available");
    uint64_t Id = 0, Other = 0; std::string Nick;
    Check(Accounts::Authenticate("fresh", "secret123", Id, Nick) == EAccountResult::NotFound, "check did not create account");
    Check(Accounts::Create("fresh", "secret123", "", Id) == EAccountResult::Success, "create");
    Check(Accounts::CheckLoginId("FRESH") == EAccountResult::DuplicateId, "case insensitive lookup");
    Check(Accounts::Create("FRESH", "secret123", "", Other) == EAccountResult::DuplicateId, "final insert prevents race");
    Check(Accounts::Authenticate("fresh", "secret123", Other, Nick) == EAccountResult::Success && Other == Id && Nick == "fresh", "manual login unchanged");
    Accounts::Stop();
    return Failed ? 1 : 0;
}
