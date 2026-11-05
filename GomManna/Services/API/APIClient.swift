import Foundation

struct APIRequestError: LocalizedError {
    let statusCode: Int
    let message: String
    var code: String? = nil
    var errorDescription: String? { message }
}

/// HTTP 처리와 공통 응답 해석만 담당합니다. 토큰 발급·저장은 호출자가 관리합니다.
struct APIClient {
    let baseURL: URL
    let session: URLSession

    init(baseURL: URL, session: URLSession = .shared) {
        self.baseURL = baseURL
        self.session = session
    }

    func request(path: String, method: String = "GET", token: String? = nil,
                 body: Data? = nil, contentType: String? = nil) -> URLRequest {
        var request = URLRequest(url: baseURL.appending(path: path))
        request.httpMethod = method
        request.timeoutInterval = 30
        request.cachePolicy = .reloadIgnoringLocalCacheData
        request.httpBody = body
        request.setValue("application/json", forHTTPHeaderField: "Accept")
        if let token { request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization") }
        if let contentType { request.setValue(contentType, forHTTPHeaderField: "Content-Type") }
        return request
    }

    func send<T: Decodable>(_ request: URLRequest, as type: T.Type) async throws -> T {
        let (data, _) = try await bytes(request)
        let response = try JSONDecoder().decode(APIResponse<T>.self, from: data)
        guard response.ok, let payload = response.data else {
            throw APIRequestError(statusCode: 200,
                                  message: response.error?.message ?? "응답 데이터를 확인해 주세요.",
                                  code: response.error?.code)
        }
        return payload
    }

    func bytes(_ request: URLRequest) async throws -> (Data, HTTPURLResponse) {
        try Task.checkCancellation()
        let data: Data
        let response: URLResponse
        do {
            (data, response) = try await session.data(for: request)
        } catch {
            // URLSession의 -999 취소도 Swift Task 취소와 동일하게 다룹니다.
            if Task.isCancelled || error.isRequestCancellation { throw CancellationError() }
            throw error
        }
        try Task.checkCancellation()
        guard let response = response as? HTTPURLResponse else { throw URLError(.badServerResponse) }
        guard (200..<300).contains(response.statusCode) else {
            let failure = try? JSONDecoder().decode(APIResponse<EmptyData>.self, from: data)
            throw APIRequestError(statusCode: response.statusCode,
                                  message: failure?.error?.message ?? "서버 요청에 실패했어요.",
                                  code: failure?.error?.code)
        }
        return (data, response)
    }

    func jsonRequest<Body: Encodable>(path: String, method: String, token: String? = nil,
                                      body: Body) throws -> URLRequest {
        request(path: path, method: method, token: token,
                body: try JSONEncoder().encode(body), contentType: "application/json")
    }
}

extension Error {
    var isRequestCancellation: Bool {
        self is CancellationError || (self as? URLError)?.code == .cancelled
    }
}
