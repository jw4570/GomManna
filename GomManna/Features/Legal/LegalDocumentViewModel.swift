import Foundation
import Observation

enum LegalDocument: String, CaseIterable, Identifiable {
    case privacy = "privacy-policy", terms = "terms-of-service", location = "location-sharing-policy", licenses = "open-source-notices"
    var id: String { rawValue }
    var title: String {
        switch self {
        case .privacy: "개인정보처리방침"
        case .terms: "서비스 이용약관"
        case .location: "위치 공유 안내"
        case .licenses: "오픈소스 라이선스"
        }
    }
}

@MainActor
@Observable final class LegalDocumentViewModel {
    var text = ""
    var error: String?
    func load(_ document: LegalDocument) {
        error = nil
        guard let url = Bundle.main.url(forResource: document.rawValue, withExtension: "md") else {
            error = "문서를 불러오지 못했어요."
            return
        }
        do {
            var content = try String(contentsOf: url, encoding: .utf8)
            for (key, value) in LegalConfiguration.replacements { content = content.replacingOccurrences(of: key, with: value) }
            text = content
        } catch { self.error = error.localizedDescription }
    }
}
