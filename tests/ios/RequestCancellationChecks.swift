import Foundation

private final class CancelledRequestProtocol: URLProtocol, @unchecked Sendable {
    nonisolated(unsafe) static var failure: URLError.Code = .cancelled
    nonisolated override class func canInit(with request: URLRequest) -> Bool { true }
    nonisolated override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
    nonisolated override func startLoading() { client?.urlProtocol(self, didFailWithError: URLError(Self.failure)) }
    nonisolated override func stopLoading() {}
}

enum RequestCancellationChecks {
    @MainActor static func run() async throws {
        precondition(CancellationError().isRequestCancellation)
        precondition(URLError(.cancelled).isRequestCancellation)
        precondition(NSError(domain: NSURLErrorDomain, code: NSURLErrorCancelled).isRequestCancellation)
        precondition(!URLError(.timedOut).isRequestCancellation)
        precondition(!APIRequestError(statusCode: 503, message: "Unavailable").isRequestCancellation)

        let configuration = URLSessionConfiguration.ephemeral
        configuration.protocolClasses = [CancelledRequestProtocol.self]
        let network = URLSession(configuration: configuration)
        defer { network.invalidateAndCancel() }
        let url = URL(string: "https://example.invalid")!
        let client = APIClient(baseURL: url, session: network)
        do {
            _ = try await client.bytes(client.request(path: "cancelled"))
            preconditionFailure("Expected cancellation")
        } catch is CancellationError {} // URLSession의 -999를 정규화해야 합니다.

        let mock = MockAPIService()
        let repositories = AppRepositories(
            auth: AuthRepository(service: mock, tokenStore: MockTokenStore(token: mock.defaultToken)),
            rooms: RoomRepository(service: RoomService(baseURL: url, session: network)),
            sharing: SharingRepository(service: mock), locations: LocationRepository(service: mock),
            notes: NoteRepository(service: mock), photos: PhotoRepository(service: mock), health: HealthRepository(service: mock))
        let suite = "GomManna.cancel-test.\(UUID())"
        let defaults = UserDefaults(suiteName: suite)!
        defer { defaults.removePersistentDomain(forName: suite) }
        let session = AppSession(repositories: repositories, defaults: defaults, storageKey: "room")
        let room = MockRoomData.room
        session.enter(RoomSession(room: room, hostUserID: room.members[0].userID, myUserID: room.members[0].userID,
                                  sharingEpoch: 1, status: "active", createdAtMs: 1, endedAtMs: nil))
        let home = HomeViewModel(session: session)
        await home.refresh()
        precondition(session.errorMessage == nil && session.current?.room.id == room.id,
                     "Cancelled navigation must preserve membership and show no alert")
        CancelledRequestProtocol.failure = .timedOut
        await home.refresh()
        precondition(session.errorMessage != nil && session.current?.room.id == room.id,
                     "Real network failures must still be reported")
        print("PASS: URLSession cancellation normalized; no home alert; membership preserved; timeout still reported")
    }
}
