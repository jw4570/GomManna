import Foundation
import Observation

/// 서버 연결 전, 현재 모임 화면이 열려 있는 동안 유지되는 대화 상태입니다.
@Observable final class ChatViewModel {
    var draft = ""
    private(set) var messages: [ChatMessage] = []
    var canSend: Bool { !draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty }

    func send() {
        let text = draft.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        messages.append(ChatMessage(text: text))
        draft = ""
    }
}
