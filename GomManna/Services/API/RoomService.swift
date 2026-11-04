import Foundation

struct APIRequestError: LocalizedError {
    let statusCode: Int
    let message: String
    
    var errorDescription: String? { message }
}

protocol RoomServiceProtocol {
    func fetchMembers(roomID: UUID, token: String) async throws -> [MemberDTO]
}

struct RoomService: RoomServiceProtocol {
    let baseURL: URL
    private let session: URLSession
    
    init(
        baseURL: URL,
        session: URLSession = .shared
    ) {
        self.baseURL = baseURL
        self.session = session
    }
    
    func fetchMembers(
        roomID: UUID,
        token: String
    ) async throws -> [MemberDTO] {
        let url = baseURL.appending(
            path: "v1/rooms/\(roomID.uuidString)/members"
        )
        
        var request = URLRequest(url: url)
        request.httpMethod = "GET"
        request.timeoutInterval = 15
        request.cachePolicy = .reloadIgnoringLocalCacheData
        
        request.setValue(
            "Bearer \(token)",
            forHTTPHeaderField: "Authorization"
        )
        
        let (data, response) = try await session.data(for: request)
        
        guard let response = response as? HTTPURLResponse else {
            throw URLError(.badServerResponse)
        }
        
        let decoder = JSONDecoder()
        
        guard (200..<300).contains(response.statusCode) else {
            let failure = try? decoder.decode(APIResponse<MembersData>.self, from: data
            )
            
            throw APIRequestError(
                statusCode: response.statusCode,
                message: failure?.error?.message ?? "멤버 정보를 가져오지 못했어요."
            )
        }
        
        let result = try decoder.decode(APIResponse<MembersData>.self, from: data)
        
        guard result.ok, let payload = result.data else {
            throw APIRequestError(statusCode: response.statusCode, message: result.error?.message ?? "응답 데이터를 확인해주세요.")
        }
        
        return payload.members
    }
}
