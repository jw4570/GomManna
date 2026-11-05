import Foundation

/// 앱에서 한 번 생성해 필요한 Repository를 화면별로 전달합니다.
@MainActor
struct AppRepositories {
    let auth: AuthRepository
    let rooms: RoomRepository
    let sharing: SharingRepository
    let locations: LocationRepository
    let notes: NoteRepository
    let photos: PhotoRepository
    let health: HealthRepository

    static func mock(service: MockAPIService) -> AppRepositories {
        AppRepositories(auth: AuthRepository(service: service, tokenStore: MockTokenStore(token: service.defaultToken)),
                        rooms: RoomRepository(service: service, initialMembers: MockRoomData.room.members),
                        sharing: SharingRepository(service: service), locations: LocationRepository(service: service),
                        notes: NoteRepository(service: service), photos: PhotoRepository(service: service),
                        health: HealthRepository(service: service))
    }

    static func live(baseURL: URL, session: URLSession = .shared) -> AppRepositories {
        AppRepositories(auth: AuthRepository(service: GuestService(baseURL: baseURL, session: session)),
                        rooms: RoomRepository(service: RoomService(baseURL: baseURL, session: session)),
                        sharing: SharingRepository(service: SharingService(baseURL: baseURL, session: session)),
                        locations: LocationRepository(service: LocationService(baseURL: baseURL, session: session)),
                        notes: NoteRepository(service: NoteService(baseURL: baseURL, session: session)),
                        photos: PhotoRepository(service: PhotoService(baseURL: baseURL, session: session)),
                        health: HealthRepository(service: HealthService(baseURL: baseURL, session: session)))
    }
}
