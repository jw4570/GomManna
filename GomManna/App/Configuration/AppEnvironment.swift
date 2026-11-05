import Foundation

@MainActor
enum AppEnvironment {
    enum DataSource { case mock, live }
    static var dataSource: DataSource {
        if ProcessInfo.processInfo.environment["XCODE_RUNNING_FOR_PREVIEWS"] == "1" { return .mock }
        return ProcessInfo.processInfo.environment["GOMMANNA_DATA_SOURCE"] == "mock" ? .mock : .live
    }
    static var baseURL: URL {
        let address = ProcessInfo.processInfo.environment["GOMMANNA_API_BASE_URL"] ?? "https://32.236.33.77"
        return URL(string: address) ?? URL(string: "https://32.236.33.77")!
    }
    static var persistenceKey: String { "GomManna.membership.\(dataSource).\(baseURL.absoluteString)" }
    private static var cachedRepositories: AppRepositories?
    static func makeRepositories() -> AppRepositories {
        if let cachedRepositories { return cachedRepositories }
        let value: AppRepositories
        switch dataSource {
        case .mock: value = .mock(service: MockAPIService())
        case .live: value = .live(baseURL: baseURL)
        }
        cachedRepositories = value
        return value
    }
}
