#pragma once
namespace MOU::ServerLog
{
void Initialize();
// 서버 PC의 로컬 시각을 비어 있지 않은 각 콘솔 줄 앞에 붙인다.
// 형식: [YYYY/MM/DD/HH시MM분SS초]. 패킷과 DB 데이터는 변경하지 않는다.
void Print(const char* Format, ...);
}
