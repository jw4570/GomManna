import Foundation

@MainActor
protocol NoteServiceProtocol {
    func updateNote(roomID: UUID, text: String, sharingEpoch: Int64, token: String) async throws -> NoteDTO
    func deleteNote(roomID: UUID, token: String) async throws
}

struct NoteService: NoteServiceProtocol {
    private let client: APIClient
    init(baseURL: URL, session: URLSession = .shared) { client = APIClient(baseURL: baseURL, session: session) }

    func updateNote(roomID: UUID, text: String, sharingEpoch: Int64, token: String) async throws -> NoteDTO {
        let request = try client.jsonRequest(path: "v1/rooms/\(roomID)/note", method: "PUT", token: token,
                                            body: NoteRequest(text: text, sharingEpoch: sharingEpoch))
        return try await client.send(request, as: NoteData.self).note
    }

    func deleteNote(roomID: UUID, token: String) async throws {
        _ = try await client.send(client.request(path: "v1/rooms/\(roomID)/note", method: "DELETE", token: token), as: EmptyData.self)
    }
}
