#pragma once
#include "Session/Session.h"

namespace MOU::ServerRuntime
{
	void BroadcastRoomMembers(uint32_t RoomId);
	void NotifyRoomClosed(const std::vector<uint64_t>& Recipients,
	                      uint32_t RoomId, ERoomCloseReason Reason);
	// [REJOIN-013] 퇴장과 릴레이 해제를 중도 입장 준비 처리와 직렬화한다.
	void LeaveRoomAndNotify(const SessionPtr& Session);
	bool HandleRoomCreateReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomListReq(const SessionPtr& Session, const char*, uint32_t);
    // [REJOIN-002] 호스트의 준비 결과를 검증하고 해당 참여자에게만 출발을 알린다.
    bool HandleRoomGuestConnectAck(const SessionPtr& Session, const char* Body, uint32_t BodySize);
    // [REJOIN-009] 입장 응답 후 게임중인 방의 호스트에게 새 접속 경로를 준비시킨다.
	bool HandleRoomJoinReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomStateUpdate(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomLeaveReq(const SessionPtr& Session, const char*, uint32_t);
	bool HandleRoomCustomizationReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	bool HandleRoomReadyReq(const SessionPtr& Session, const char* Body, uint32_t BodySize);
	// [REJOIN-014] 최초 게임 시작과 중도 입장의 경로 할당 순서를 보장한다.
	bool HandleRoomStartReq(const SessionPtr& Session, const char*, uint32_t);
	// [REJOIN-015] 최초 참여자에게만 호스트의 최초 준비 신호를 전달한다.
	bool HandleRoomHostReadyReq(const SessionPtr& Session, const char*, uint32_t);
}
