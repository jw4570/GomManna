import Foundation
import Observation

/// 현재 사용자의 프로필. 다른 멤버의 캐릭터는 이 상태를 사용하지 않습니다.
@Observable final class ProfileViewModel {
    static let shared = ProfileViewModel()
    private(set) var character: AnimalCharacter
    private let defaults: UserDefaults
    private static let storageKey = "GomManna.profile.character"

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        if let saved = defaults.string(forKey: Self.storageKey), let animal = AnimalCharacter(rawValue: saved) {
            character = animal
        } else {
            let animal = AnimalCharacter.random()
            character = animal
            defaults.set(animal.rawValue, forKey: Self.storageKey)
        }
    }

    func select(_ animal: AnimalCharacter) {
        character = animal
        defaults.set(animal.rawValue, forKey: Self.storageKey)
    }
}
