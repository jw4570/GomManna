import SwiftUI

struct RoomSettingsView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var model = RoomSettingsViewModel()
    let room: Room
    let onRemove: () -> Void
    var session: AppSession = .shared
    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(room.title).font(.title2.bold())
                        Text("\(room.members.count)명 참여 중").foregroundStyle(.secondary).font(.subheadline)
                    }.padding(.vertical, 12)
                }
                Section {
                    NavigationLink { InviteCodeView(room: room, session: session) } label: { Label("초대 코드 보기", systemImage: "qrcode") }
                } footer: { Text("QR과 코드로 함께할 사람들을 초대하세요.") }
                if let error = model.error { Text(error).foregroundStyle(.red) }
                Section {
                    Button(role: .destructive) { model.confirmingRemoval = true } label: {
                        Label(model.isLoading ? "처리 중…" : (room.isHost ? "모임 종료" : "모임 나가기"), systemImage: room.isHost ? "trash" : "rectangle.portrait.and.arrow.right")
                    }
                }
            }
            .disabled(model.isLoading)
            .interactiveDismissDisabled(model.isLoading)
            .navigationTitle("방 설정").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("완료") { dismiss() } } }
            .confirmationDialog(room.isHost ? "모임을 종료할까요?" : "모임에서 나갈까요?", isPresented: $model.confirmingRemoval, titleVisibility: .visible) {
                Button(room.isHost ? "모임 종료" : "모임 나가기", role: .destructive) {
                    Task { if await model.remove(room: room, session: session) { dismiss(); onRemove() } }
                }
            } message: { Text(room.isHost ? "모임이 종료되고 홈에서 사라져요." : "다시 참여하려면 초대 코드가 필요해요.") }
        }
    }
}
#Preview { RoomSettingsView(room: .sample) {} }
