import SwiftUI

struct IdentityRecoveryButton: View {
    let session: AppSession
    @State private var confirming = false
    var body: some View {
        Button("새 익명 사용자로 시작") { confirming = true }
            .disabled(session.preparing)
            .confirmationDialog("새 사용자로 시작할까요?", isPresented: $confirming, titleVisibility: .visible) {
                Button("새 사용자 생성", role: .destructive) { Task { await session.recoverIdentity() } }
            } message: { Text("기존 사용자로 복구할 수 없어요. 기존 모임에 다시 참여하려면 초대 코드가 필요해요.") }
    }
}
