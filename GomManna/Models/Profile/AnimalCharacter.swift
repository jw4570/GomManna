import Foundation

enum AnimalCharacter: String, CaseIterable, Identifiable {
    case bear
    case rabbit
    case fox
    case cat
    case dog
    case panda
    case koala
    case tiger
    case penguin
    var id: String { rawValue }
    var emoji: String {
        switch self {
        case .bear: "🐻"
        case .rabbit: "🐰"
        case .fox: "🦊"
        case .cat: "🐱"
        case .dog: "🐶"
        case .panda: "🐼"
        case .koala: "🐨"
        case .tiger: "🐯"
        case .penguin: "🐧"
        }
    }
    var name: String {
        switch self {
        case .bear: "곰"
        case .rabbit: "토끼"
        case .fox: "여우"
        case .cat: "고양이"
        case .dog: "강아지"
        case .panda: "판다"
        case .koala: "코알라"
        case .tiger: "호랑이"
        case .penguin: "펭귄"
        }
    }
    static func random() -> AnimalCharacter { allCases.randomElement() ?? .bear }
}
