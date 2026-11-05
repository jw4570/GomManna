import Foundation

/// 모든 Mock Repository가 같은 인스턴스를 공유합니다. 네트워크와 실제 Keychain은 사용하지 않습니다.
@MainActor
final class MockAPIService: RoomManagingServiceProtocol, GuestServiceProtocol, SharingServiceProtocol,
                            LocationServiceProtocol, NoteServiceProtocol, PhotoServiceProtocol, HealthServiceProtocol {
    struct MemberRecord {
        var member: MemberDTO
        var location: LocationDTO?
        var note: NoteDTO?
        var photo: Data?
    }
    struct RoomRecord {
        var room: RoomDTO
        var members: [MemberRecord]
    }
    private var rooms: [UUID: RoomRecord] = [:]
    private var users: [String: UUID] = [:]
    let defaultToken = "mock-token"
    private var now: Int64 { Int64(Date().timeIntervalSince1970 * 1000) }

    init(room: Room? = nil) {
        seed(room ?? MockRoomData.room)
    }

    /// 기존 로컬 UI에서 만든 방도 같은 저장소에 등록합니다. 이미 존재하면 덮어쓰지 않습니다.
    func seed(_ room: Room) {
        guard rooms[room.id] == nil, let host = room.members.first(where: \.isHost) else { return }
        let records = room.members.map { member in
            let location: LocationDTO?
            if member.isSharing, let latitude = member.latitude, let longitude = member.longitude {
                location = LocationDTO(latitude: latitude, longitude: longitude, accuracyM: 10,
                                       capturedAtMs: now, receivedAtMs: now, sharingEpoch: 1, sequence: 1)
            } else { location = nil }
            let dto = MemberDTO(memberID: member.id, roomID: room.id, userID: member.userID,
                                nickname: member.name, isSharing: member.isSharing, sharingEpoch: 1,
                                joinedAtMs: now, locationStatus: nil, location: nil, note: nil)
            return MemberRecord(member: dto, location: location)
        }
        rooms[room.id] = RoomRecord(room: RoomDTO(roomID: room.id, title: room.title, hostID: host.userID,
                                                inviteCode: room.code, status: "active", createdAtMs: now, endedAtMs: nil),
                                   members: records)
        if let me = room.members.first(where: { $0.id == room.myMemberID }) {
            users[defaultToken] = me.userID
        }
    }

    func createGuest() async throws -> GuestDTO {
        try Task.checkCancellation()
        let token = "mock-\(UUID().uuidString)"
        let id = UUID()
        users[token] = id
        return GuestDTO(userID: id, token: token, expiresAtMs: now + 30 * 24 * 60 * 60 * 1000)
    }

    func createRoom(title: String, nickname: String, token: String) async throws -> RoomMembershipData {
        let user = try actor(token)
        guard !title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
              !nickname.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            throw failure(400, "INVALID_ARGUMENT", "방 제목과 이름을 입력해 주세요.")
        }
        let alphabet = Array("ABCDEFGHJKLMNPQRSTUVWXYZ23456789")
        var code: String
        repeat { code = String((0..<6).map { _ in alphabet.randomElement()! }) }
        while rooms.values.contains(where: { $0.room.inviteCode == code })
        let id = UUID()
        let room = RoomDTO(roomID: id, title: title, hostID: user, inviteCode: code,
                           status: "active", createdAtMs: now, endedAtMs: nil)
        let member = makeMember(roomID: id, userID: user, nickname: nickname)
        rooms[id] = RoomRecord(room: room, members: [MemberRecord(member: member)])
        return RoomMembershipData(room: room, member: member)
    }

    func joinRoom(inviteCode: String, nickname: String, token: String) async throws -> RoomMembershipData {
        let user = try actor(token)
        guard !nickname.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            throw failure(400, "INVALID_ARGUMENT", "이름을 입력해 주세요.")
        }
        let code = inviteCode.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
        guard var record = rooms.values.first(where: { $0.room.inviteCode == code && $0.room.status == "active" }) else {
            throw failure(404, "NOT_FOUND", "초대 코드를 확인해 주세요.")
        }
        if let existing = record.members.first(where: { $0.member.userID == user }) {
            return RoomMembershipData(room: record.room, member: visible(existing))
        }
        guard record.members.count < 8 else { throw failure(409, "CONFLICT", "모임 정원이 찼어요.") }
        let member = makeMember(roomID: record.room.roomID, userID: user, nickname: nickname)
        record.members.append(MemberRecord(member: member))
        rooms[record.room.roomID] = record
        return RoomMembershipData(room: record.room, member: member)
    }

    func fetchRoom(roomID: UUID, token: String) async throws -> RoomDTO {
        try authorized(roomID, token).room
    }

    func fetchMembers(roomID: UUID, token: String) async throws -> [MemberDTO] {
        try authorized(roomID, token).members.map(visible)
    }

    func fetchInvite(roomID: UUID, token: String) async throws -> InviteDTO {
        let room = try authorized(roomID, token).room
        let payload = "gommanna://join?code=\(room.inviteCode)"
        return InviteDTO(inviteCode: room.inviteCode, qrPayload: payload, qrSVG: try MockQRCode.svg(payload))
    }

    func leaveRoom(roomID: UUID, token: String) async throws {
        var record = try authorized(roomID, token)
        let user = try actor(token)
        guard record.room.hostID != user else { throw failure(403, "FORBIDDEN", "방장은 모임 종료를 사용해 주세요.") }
        record.members.removeAll { $0.member.userID == user }
        rooms[roomID] = record
    }

    func endRoom(roomID: UUID, token: String) async throws {
        var record = try authorized(roomID, token)
        guard record.room.hostID == (try actor(token)) else { throw failure(403, "FORBIDDEN", "방장만 종료할 수 있어요.") }
        let old = record.room
        record.room = RoomDTO(roomID: old.roomID, title: old.title, hostID: old.hostID, inviteCode: old.inviteCode,
                              status: "ended", createdAtMs: old.createdAtMs, endedAtMs: now)
        record.members.removeAll()
        rooms[roomID] = record
    }

    func updateSharing(roomID: UUID, enabled: Bool, token: String) async throws -> MemberDTO {
        var record = try authorized(roomID, token)
        let index = try memberIndex(record, token)
        let old = record.members[index].member
        let epoch = old.sharingEpoch + ((!old.isSharing && enabled) ? 1 : 0)
        record.members[index].member = MemberDTO(memberID: old.memberID, roomID: roomID, userID: old.userID,
                                                nickname: old.nickname, isSharing: enabled, sharingEpoch: epoch,
                                                joinedAtMs: old.joinedAtMs, locationStatus: nil, location: nil, note: nil)
        if !enabled || epoch != old.sharingEpoch {
            record.members[index].location = nil
            record.members[index].note = nil
            record.members[index].photo = nil
        }
        rooms[roomID] = record
        return visible(record.members[index])
    }

    func updateLocation(roomID: UUID, location: LocationRequest, token: String) async throws -> LocationDTO {
        var record = try authorized(roomID, token)
        let index = try memberIndex(record, token)
        try checkSharing(record.members[index], epoch: location.sharingEpoch)
        guard location.latitude.isFinite, location.longitude.isFinite,
              abs(location.latitude) <= 90, abs(location.longitude) <= 180,
              location.sequence > 0, location.capturedAtMs <= now + 60_000,
              location.accuracyM.map({ $0.isFinite && $0 >= 0 }) ?? true else {
            throw failure(400, "INVALID_ARGUMENT", "위치 정보를 확인해 주세요.")
        }
        if let previous = record.members[index].location,
           location.sequence <= previous.sequence || location.capturedAtMs < previous.capturedAtMs {
            throw failure(409, "STALE_UPDATE", "이전 위치 업데이트예요.")
        }
        let result = LocationDTO(latitude: location.latitude, longitude: location.longitude, accuracyM: location.accuracyM,
                                 capturedAtMs: location.capturedAtMs, receivedAtMs: now,
                                 sharingEpoch: location.sharingEpoch, sequence: location.sequence)
        record.members[index].location = result
        rooms[roomID] = record
        return result
    }

    func updateNote(roomID: UUID, text: String, sharingEpoch: Int64, token: String) async throws -> NoteDTO {
        var record = try authorized(roomID, token)
        let index = try memberIndex(record, token)
        try checkSharing(record.members[index], epoch: sharingEpoch)
        guard let location = record.members[index].location else {
            throw failure(409, "CONFLICT", "메모 작성 전 위치를 전송해 주세요.")
        }
        let note = NoteDTO(noteID: UUID(), text: text, writtenAtMs: now,
                           origin: NoteOriginDTO(latitude: location.latitude, longitude: location.longitude, capturedAtMs: location.capturedAtMs),
                           sharingEpoch: sharingEpoch, photoAvailable: false, photoUploadedAtMs: nil, photoPath: nil)
        record.members[index].note = note
        record.members[index].photo = nil
        rooms[roomID] = record
        return note
    }

    func deleteNote(roomID: UUID, token: String) async throws {
        var record = try authorized(roomID, token)
        let index = try memberIndex(record, token)
        record.members[index].note = nil
        record.members[index].photo = nil
        rooms[roomID] = record
    }

    func uploadPhoto(roomID: UUID, noteID: UUID, writtenAtMs: Int64, sharingEpoch: Int64,
                     imageData: Data, format: PhotoService.Format, token: String) async throws -> NoteDTO {
        var record = try authorized(roomID, token)
        let index = try memberIndex(record, token)
        try checkSharing(record.members[index], epoch: sharingEpoch)
        guard let note = record.members[index].note, note.noteID == noteID, note.writtenAtMs == writtenAtMs else {
            throw failure(409, "STALE_UPDATE", "메모가 변경되었어요.")
        }
        guard !imageData.isEmpty, imageData.count <= 5 * 1024 * 1024 else {
            throw failure(413, "PHOTO_TOO_LARGE", "사진은 5 MiB 이하여야 해요.")
        }
        // 실제 이미지 검사·재인코딩은 서버 책임입니다. Mock은 입력 바이트를 메모리에 보관합니다.
        let updated = NoteDTO(noteID: note.noteID, text: note.text, writtenAtMs: note.writtenAtMs, origin: note.origin,
                              sharingEpoch: note.sharingEpoch, photoAvailable: true, photoUploadedAtMs: now,
                              photoPath: "/v1/rooms/\(roomID)/members/\(record.members[index].member.memberID)/photo")
        record.members[index].note = updated
        record.members[index].photo = imageData
        rooms[roomID] = record
        return updated
    }

    func fetchPhoto(roomID: UUID, memberID: UUID, token: String) async throws -> Data {
        let record = try authorized(roomID, token)
        guard let member = record.members.first(where: { $0.member.memberID == memberID }),
              visible(member).note?.photoAvailable == true, let photo = member.photo else {
            throw failure(404, "NOT_FOUND", "공유된 사진이 없어요.")
        }
        return photo
    }

    func fetchHealth() async throws -> HealthDTO { HealthDTO(status: "ok") }

    private func actor(_ token: String) throws -> UUID {
        try Task.checkCancellation()
        guard let user = users[token] else { throw failure(401, "UNAUTHENTICATED", "Mock 토큰이 유효하지 않아요.") }
        return user
    }
    private func authorized(_ id: UUID, _ token: String) throws -> RoomRecord {
        let user = try actor(token)
        guard let room = rooms[id] else { throw failure(404, "NOT_FOUND", "모임을 찾을 수 없어요.") }
        guard room.room.status == "active" else { throw failure(410, "ROOM_CLOSED", "종료된 모임이에요.") }
        guard room.members.contains(where: { $0.member.userID == user }) else { throw failure(403, "FORBIDDEN", "모임 참가자가 아니에요.") }
        return room
    }
    private func memberIndex(_ room: RoomRecord, _ token: String) throws -> Int {
        let user = try actor(token)
        guard let index = room.members.firstIndex(where: { $0.member.userID == user }) else {
            throw failure(403, "FORBIDDEN", "모임 참가자가 아니에요.")
        }
        return index
    }
    private func checkSharing(_ record: MemberRecord, epoch: Int64) throws {
        guard record.member.isSharing else { throw failure(409, "SHARING_OFF", "위치 공유가 꺼져 있어요.") }
        guard record.member.sharingEpoch == epoch else { throw failure(409, "STALE_UPDATE", "공유 회차가 변경되었어요.") }
    }
    private func makeMember(roomID: UUID, userID: UUID, nickname: String) -> MemberDTO {
        MemberDTO(memberID: UUID(), roomID: roomID, userID: userID, nickname: nickname, isSharing: false,
                  sharingEpoch: 0, joinedAtMs: now, locationStatus: nil, location: nil, note: nil)
    }
    private func visible(_ record: MemberRecord) -> MemberDTO {
        let m = record.member
        let location = m.isSharing && record.location?.sharingEpoch == m.sharingEpoch ? record.location : nil
        let note = m.isSharing && record.note?.sharingEpoch == m.sharingEpoch ? record.note : nil
        let status: LocationStatus
        if !m.isSharing { status = .off }
        else if let location {
            let age = now - location.capturedAtMs
            status = age > 120_000 ? .stale : age > 30_000 ? .delayed : .fresh
        } else { status = .noData }
        return MemberDTO(memberID: m.memberID, roomID: m.roomID, userID: m.userID, nickname: m.nickname,
                         isSharing: m.isSharing, sharingEpoch: m.sharingEpoch, joinedAtMs: m.joinedAtMs,
                         locationStatus: status, location: location, note: note)
    }
    private func failure(_ status: Int, _ code: String, _ message: String) -> APIRequestError {
        APIRequestError(statusCode: status, message: message, code: code)
    }
}

@MainActor
final class MockTokenStore: TokenStoring {
    private var token: String?
    init(token: String? = nil) { self.token = token }
    func read() throws -> String? { token }
    func save(_ token: String) throws { self.token = token }
}
