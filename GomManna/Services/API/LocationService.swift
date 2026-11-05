import Foundation

@MainActor
protocol LocationServiceProtocol {
    func updateLocation(roomID: UUID, location: LocationRequest, token: String) async throws -> LocationDTO
}

struct LocationService: LocationServiceProtocol {
    private let client: APIClient
    init(baseURL: URL, session: URLSession = .shared) { client = APIClient(baseURL: baseURL, session: session) }

    /// 같은 공유 epoch 내에서 sequence를 증가시켜 전달합니다.
    func updateLocation(roomID: UUID, location: LocationRequest, token: String) async throws -> LocationDTO {
        let request = try client.jsonRequest(path: "v1/rooms/\(roomID)/location", method: "PUT", token: token, body: location)
        return try await client.send(request, as: LocationData.self).location
    }
}
