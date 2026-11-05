import Foundation

@MainActor
protocol GuestServiceProtocol {
    func createGuest() async throws -> GuestDTO
}

struct GuestService: GuestServiceProtocol {
    private let client: APIClient
    init(baseURL: URL, session: URLSession = .shared) { client = APIClient(baseURL: baseURL, session: session) }

    func createGuest() async throws -> GuestDTO {
        let request = try client.jsonRequest(path: "v1/guests", method: "POST", body: EmptyData())
        return try await client.send(request, as: GuestDTO.self)
    }
}
