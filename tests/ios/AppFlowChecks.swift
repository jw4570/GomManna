import Foundation

/// CLI 통합 검증용입니다. 앱 타깃에 포함되지 않습니다.
@main struct AppFlowChecks {
    @MainActor static func main() async throws {
        let live = CommandLine.arguments.contains("--live")
        if !live { try await IntroChecks.run(); RoomMapChecks.run(); MapNotePhotoChecks.run(); try await RequestCancellationChecks.run(); try await BackgroundLocationChecks.run() }
        let service = MockAPIService()
        let url = URL(string: "https://32.236.33.77")!
        func repositories() -> AppRepositories {
            if live {
                return AppRepositories(auth: AuthRepository(service: GuestService(baseURL: url), tokenStore: MockTokenStore()),
                    rooms: RoomRepository(service: RoomService(baseURL: url)), sharing: SharingRepository(service: SharingService(baseURL: url)),
                    locations: LocationRepository(service: LocationService(baseURL: url)), notes: NoteRepository(service: NoteService(baseURL: url)),
                    photos: PhotoRepository(service: PhotoService(baseURL: url)), health: HealthRepository(service: HealthService(baseURL: url)))
            }
            return AppRepositories(auth: AuthRepository(service: service, tokenStore: MockTokenStore()),
                rooms: RoomRepository(service: service), sharing: SharingRepository(service: service),
                locations: LocationRepository(service: service), notes: NoteRepository(service: service),
                photos: PhotoRepository(service: service), health: HealthRepository(service: service))
        }
        let suite = "GomManna.flow-test.\(UUID())"
        let defaults = UserDefaults(suiteName: suite)!
        defer { defaults.removePersistentDomain(forName: suite) }
        let hostRepos = repositories()
        let participantRepos = repositories()
        let host = AppSession(repositories: hostRepos, defaults: defaults, storageKey: "host")
        let participant = AppSession(repositories: participantRepos, defaults: defaults, storageKey: "participant")
        await host.prepare()
        await participant.prepare()
        precondition(host.ready && participant.ready, "Session preparation failed")
        let creator = CreateRoomViewModel(session: host)
        creator.title = "곰만나 자동 연동 검증 \(UUID().uuidString.prefix(6))"
        creator.name = "테스트 방장"
        guard let room = await creator.create() else { fatalError("Room creation failed: \(creator.error ?? "")") }
        let hostToken = try await hostRepos.auth.accessToken()
        do {
            let invitation = try await hostRepos.rooms.fetchInvite(roomID: room.id, token: hostToken)
            let join = JoinRoomViewModel(session: participant)
            join.scanned(invitation.qrPayload)
            join.name = "테스트 참가자"
            precondition(join.canEnter)
            guard await join.join() != nil else { fatalError("Join failed: \(join.error ?? "")") }
            try await host.refresh()
            precondition(host.current?.room.members.count == 2)
            let restored = AppSession(repositories: hostRepos, defaults: defaults, storageKey: "host")
            await restored.prepare()
            precondition(restored.current?.room.id == room.id)
            let sharing = try await hostRepos.sharing.updateSharing(roomID: room.id, enabled: true, token: hostToken)
            _ = try await hostRepos.locations.updateLocation(roomID: room.id, latitude: 37.5665, longitude: 126.978,
                accuracyMeters: 10, capturedAtMs: Int64(Date().timeIntervalSince1970 * 1000), sharingEpoch: sharing.epoch, sequence: 1, token: hostToken)
            let note = try await hostRepos.notes.updateNote(roomID: room.id, text: "자동 검증용 메모", sharingEpoch: sharing.epoch, token: hostToken)
            let png = Data(base64Encoded: "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aN1sAAAAASUVORK5CYII=")!
            let attached = try await hostRepos.photos.uploadPhoto(roomID: room.id, note: note, imageData: png, format: .png, token: hostToken)
            precondition(attached.photoAvailable)
            let participantToken = try await participantRepos.auth.accessToken()
            let data = try await participantRepos.photos.fetchPhoto(roomID: room.id, memberID: host.current!.room.myMemberID, token: participantToken)
            precondition(!data.isEmpty)
            let settings = SettingsViewModel(session: host)
            await settings.stopSharing()
            precondition(settings.error == nil && settings.me?.isSharing == false)
            precondition(settings.supportURL?.scheme == "mailto")
            try await participant.refresh()
            let other = participant.current!.room.members.first { $0.isHost }!
            precondition(other.latitude == nil && other.note == nil)
            await settings.deleteNote()
            precondition(settings.error == nil)
            let leave = RoomSettingsViewModel()
            let left = await leave.remove(room: participant.current!.room, session: participant)
            precondition(left)
            precondition(participant.current == nil)
            let end = RoomSettingsViewModel()
            let ended = await end.remove(room: host.current!.room, session: host)
            precondition(ended)
            precondition(host.current == nil)
            let closedRestore = AppSession(repositories: hostRepos, defaults: defaults, storageKey: "host")
            await closedRestore.prepare()
            precondition(closedRestore.current == nil)
            print("PASS \(live ? "LIVE" : "MOCK"): auth, create/join ViewModels, invite, membership restore, location, note/photo, sharing OFF, leave/end; test room closed")
        } catch {
            try? await hostRepos.rooms.endRoom(roomID: room.id, token: hostToken)
            throw error
        }
    }
}
