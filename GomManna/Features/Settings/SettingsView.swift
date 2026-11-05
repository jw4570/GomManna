import SwiftUI

struct SettingsView: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(\.scenePhase) private var scenePhase
    @Environment(\.openURL) private var openURL
    @AppStorage("GomManna.appearance") private var appearance = "system"
    @State private var showingProfile = false
    @State private var managingRoom = false
    @State private var model: SettingsViewModel
    init(session: AppSession) { _model = State(initialValue: SettingsViewModel(session: session)) }
    var body: some View {
        NavigationStack {
            List {
                Section {
                    Button { showingProfile = true } label: { Label("내 동물 프로필", systemImage: "person.crop.circle") }
                    Picker(selection: $appearance) {
                        Text("기기 설정").tag("system")
                        Text("라이트").tag("light")
                        Text("다크").tag("dark")
                    } label: { Label("화면 모드", systemImage: "circle.lefthalf.filled") }
                } header: { Text("내 설정") }
                Section {
                    LabeledContent("위치", value: model.locationPermission)
                    LabeledContent("카메라", value: model.cameraPermission)
                    Button { openSystemSettings() } label: { Label("기기에서 권한 변경", systemImage: "gearshape") }
                } header: { Text("앱 권한") } footer: {
                    Text("위치는 공유를 켠 동안 화면을 잠그거나 다른 앱을 사용할 때도 공유해요. 끄려면 내 데이터 관리에서 공유를 중단해 주세요. 카메라는 초대 QR을 스캔할 때 사용해요. 사진은 직접 선택한 항목만 가져와요. 푸시 알림은 사용하지 않아요.")
                }
                Section {
                    NavigationLink { DataManagementView(model: model) } label: { Label("내 데이터 관리", systemImage: "hand.raised") }
                    if model.room != nil {
                        Button { managingRoom = true } label: { Label("참여 중인 모임 관리", systemImage: "person.2") }
                    }
                } header: { Text("개인정보와 모임") }
                Section {
                    NavigationLink { LegalDocumentView(document: .privacy) } label: { Text("개인정보처리방침") }
                    NavigationLink { LegalDocumentView(document: .terms) } label: { Text("서비스 이용약관") }
                    NavigationLink { LegalDocumentView(document: .location) } label: { Text("위치 공유 안내") }
                    NavigationLink { LegalDocumentView(document: .licenses) } label: { Text("오픈소스 라이선스") }
                } header: { Text("약관 및 정책") }
                Section {
                    if let url = model.supportURL {
                        Button { openURL(url) { accepted in if !accepted { model.error = "메일 앱을 열 수 없어요. 문의 이메일 주소를 복사해 이용해 주세요." } } }
                            label: { Label("문의·오류 제보", systemImage: "envelope") }
                        Text(LegalConfiguration.supportEmail ?? "").font(.subheadline).textSelection(.enabled)
                    } else {
                        LabeledContent("문의 이메일", value: "등록 준비 중")
                    }
                    NavigationLink { AboutAppView(version: model.version) } label: { Label("앱 정보", systemImage: "info.circle") }
                    LabeledContent("버전", value: model.version)
                } header: { Text("도움말") }
            }
            .scrollContentBackground(.hidden).background(AppStyle.background)
            .navigationTitle("설정").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("완료") { dismiss() } } }
            .sheet(isPresented: $showingProfile) { ProfileView() }
            .sheet(isPresented: $managingRoom) {
                if let room = model.room {
                    RoomSettingsView(room: room, onRemove: { managingRoom = false }, session: model.session)
                }
            }
            .task { model.readPermissions() }
            .onChange(of: scenePhase) { _, phase in if phase == .active { model.readPermissions() } }
            .alert("안내", isPresented: Binding(get: { model.error != nil }, set: { if !$0 { model.error = nil } })) {
                Button("확인") { model.error = nil }
            } message: { Text(model.error ?? "") }
        }.tint(AppStyle.accent)
    }
    private func openSystemSettings() {
        if let url = URL(string: UIApplication.openSettingsURLString) { openURL(url) }
    }
}

private struct AboutAppView: View {
    let version: String
    var body: some View {
        List {
            Section {
                VStack(spacing: 18) {
                    BrandMark(size: 90)
                    Text("곰만나").font(.title.bold())
                    Text("우리 곧, 만나요").foregroundStyle(.secondary)
                    Text(version).font(.caption).foregroundStyle(.secondary)
                }.frame(maxWidth: .infinity).padding(.vertical, 24)
            }
            Section {
                Text("즉석 모임을 만들고 초대 코드로 함께해요. 내가 켠 동안만 위치를 공유하고, 메모와 사진으로 만날 장소를 알려요.")
                if let name = LegalConfiguration.operatorName { LabeledContent("운영자", value: name) }
            }
        }.navigationTitle("앱 정보").navigationBarTitleDisplayMode(.inline)
    }
}

#Preview { SettingsView(session: .preview()) }
