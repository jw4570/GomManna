import Foundation

struct APIResponse<T: Decodable>: Decodable {
    let ok: Bool
    let data: T?
    let error: APIErrorDTO?
}

struct APIErrorDTO: Decodable {
    let code: String
    let message: String
}
