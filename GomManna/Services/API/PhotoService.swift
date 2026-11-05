import Foundation

@MainActor
protocol PhotoServiceProtocol {
    func uploadPhoto(roomID: UUID, noteID: UUID, writtenAtMs: Int64, sharingEpoch: Int64, imageData: Data, format: PhotoService.Format, token: String) async throws -> NoteDTO
    func fetchPhoto(roomID: UUID, memberID: UUID, token: String) async throws -> Data
}

struct PhotoService: PhotoServiceProtocol {
    enum Format {
        case jpeg, png
        var mimeType: String { self == .jpeg ? "image/jpeg" : "image/png" }
        var filename: String { self == .jpeg ? "photo.jpg" : "photo.png" }
    }

    private let client: APIClient
    init(baseURL: URL, session: URLSession = .shared) { client = APIClient(baseURL: baseURL, session: session) }

    /// 이미지 방향 보정은 호출 전에 수행해야 합니다. 픽셀 크기·내용 검사는 서버가 수행합니다.
    func uploadPhoto(roomID: UUID, noteID: UUID, writtenAtMs: Int64, sharingEpoch: Int64,
                     imageData: Data, format: Format, token: String) async throws -> NoteDTO {
        guard !imageData.isEmpty, imageData.count <= 5 * 1024 * 1024 else {
            throw APIRequestError(statusCode: 413, message: "사진은 비어 있지 않은 5 MiB 이하 파일이어야 해요.", code: "PHOTO_TOO_LARGE")
        }
        let boundary = "GomManna-\(UUID().uuidString)"
        var body = Data()
        func append(_ text: String) { body.append(Data(text.utf8)) }
        for (name, value) in [("note_id", noteID.uuidString), ("written_at_ms", String(writtenAtMs)),
                              ("sharing_epoch", String(sharingEpoch))] {
            append("--\(boundary)\r\nContent-Disposition: form-data; name=\"\(name)\"\r\n\r\n\(value)\r\n")
        }
        append("--\(boundary)\r\nContent-Disposition: form-data; name=\"file\"; filename=\"\(format.filename)\"\r\nContent-Type: \(format.mimeType)\r\n\r\n")
        body.append(imageData)
        append("\r\n--\(boundary)--\r\n")
        let request = client.request(path: "v1/rooms/\(roomID)/note/photo", method: "PUT", token: token,
                                     body: body, contentType: "multipart/form-data; boundary=\(boundary)")
        return try await client.send(request, as: NoteData.self).note
    }

    func fetchPhoto(roomID: UUID, memberID: UUID, token: String) async throws -> Data {
        var request = client.request(path: "v1/rooms/\(roomID)/members/\(memberID)/photo", token: token)
        request.setValue("image/jpeg, image/png", forHTTPHeaderField: "Accept")
        let (data, _) = try await client.bytes(request)
        return data
    }
}
