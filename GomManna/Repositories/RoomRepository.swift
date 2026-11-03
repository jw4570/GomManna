import Foundation

@MainActor
final class RoomRepository {
    private let service: any RoomServiceProtocol
    
    private var characters: [UUID: AnimalCharacter] = [:]
    
    init(service: any RoomServiceProtocol, initialMembers: [RoomMember] = []) {
        self.service = service
        for member in initialMembers {
            characters[member.userID] = member.character
        }
    }
    
    func fetchMembers(roomID: UUID, hostUserID: UUID, token: String) async throws -> [RoomMember] {
        let dtos = try await service.fetchMembers(roomID: roomID, token: token)
        
        return dtos.map { dto in
            let character: AnimalCharacter
            
            if let saved = characters[dto.userID] {
                character = saved
            } else {
                character = .random()
                characters[dto.userID] = character
            }
            
            let location = dto.isSharing ? dto.location : nil
            
            return RoomMember(
                id: dto.memberID,
                userID: dto.userID,
                name: dto.nickname,
                character: character,
                isHost: dto.userID == hostUserID,
                isSharing: dto.isSharing,
                latitude: location?.latitude,
                longitude: location?.longitude
            )
        }
    }
}
