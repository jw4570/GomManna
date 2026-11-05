import SwiftUI

struct DataManagementView: View {
    @Environment(\.openURL) private var openURL
    @Bindable var model: SettingsViewModel
    @State private var confirmingNoteDeletion = false
    var body: some View {
        List {
            Section {
                if let room = model.room {
                    LabeledContent("참여 중인 모임", value: room.title)
                    LabeledContent("내 위치 공유", value: model.me?.isSharing == true ? "켜짐" : "꺼짐")
                    Button("위치 공유 끄기") { Task { await model.stopSharing() } }
                        .disabled(model.me?.isSharing != true || model.busy)
                    Button("이 모임의 내 메모·사진 삭제", role: .destructive) { confirmingNoteDeletion = true }
                        .disabled(model.busy)
                } else { Text("현재 참여 중인 모임이 없어요.").foregroundStyle(.secondary) }
            } header: { Text("공유 중인 데이터") } footer: {
                Text("공유를 켜면 홈이나 잠금 화면에서도 계속 공유해요. 공유를 끄면 새 위치 전송과 다른 멤버의 조회가 중단돼요. 공유 중단과 저장된 데이터의 삭제는 달라요. 이전에 다른 사람이 저장하거나 캡처한 내용까지 지울 수는 없어요.")
            }
            Section {
                Text("익명 사용자와 전체 데이터 삭제") .font(.headline)
                Text("전체 사용자 삭제 기능은 준비 중이에요. 지금은 내 메모·사진 삭제와 모임 나가기·종료를 이용할 수 있어요. 이는 전체 사용자 삭제를 대신하지 않아요.")
                if let email = LegalConfiguration.supportEmail { Text("개인정보 관련 문의: \(email)").textSelection(.enabled) }
                if let url = model.supportURL { Button("개인정보 문의 메일 작성") { openURL(url) } }
                if let id = model.session.current?.myUserID {
                    LabeledContent("문의용 사용자 ID") { Text(id.uuidString).font(.caption).textSelection(.enabled) }
                }
            } footer: { Text("앱 삭제나 새 익명 사용자 생성만으로 서버의 기존 사용자와 모든 데이터가 삭제되지는 않아요.") }
            Section {
                Text("인증 정보는 기기의 Keychain에, 현재 모임 정보와 화면 설정은 기기에 저장해요. 동물 프로필은 이 기기에서만 적용돼요.")
            } header: { Text("기기에 저장되는 정보") }
            NavigationLink("개인정보처리방침 보기") { LegalDocumentView(document: .privacy) }
        }
        .navigationTitle("내 데이터 관리").navigationBarTitleDisplayMode(.inline)
        .confirmationDialog("메모와 첨부 사진을 삭제할까요?", isPresented: $confirmingNoteDeletion, titleVisibility: .visible) {
            Button("메모·사진 삭제", role: .destructive) { Task { await model.deleteNote() } }
        } message: { Text("현재 모임의 내 메모와 첨부 사진을 삭제해요. 사진 파일의 정리에는 시간이 걸릴 수 있어요.") }
    }
}
