import SwiftUI

struct RootView: View {
    @Environment(\.scenePhase) private var scenePhase
    @State private var intro = IntroViewModel()
    @State private var session = AppSession.shared
    @State private var legalDocument: LegalDocument?
    var body: some View {
        ZStack {
            if !intro.isFinished { IntroView() }
            else if session.ready { HomeView(session: session) }
            else {
                VStack(spacing: 24) {
                    BrandMark(size: 90)
                    if let error = session.errorMessage {
                        Text(error).multilineTextAlignment(.center)
                        Button("다시 연결") { Task { await session.prepare() } }.disabled(session.preparing)
                        if session.recoveryNeeded { IdentityRecoveryButton(session: session) }
                    } else { ProgressView("만날 준비를 하고 있어요") }
                }.padding(32)
            }
        }
        .background(AppStyle.background)
        .safeAreaInset(edge: .bottom) {
            if !session.ready && intro.isFinished {
                HStack(spacing: 24) {
                    Button("개인정보처리방침") { legalDocument = .privacy }
                    Button("이용약관") { legalDocument = .terms }
                }.font(.footnote).padding()
            }
        }
        .sheet(item: $legalDocument) { document in
            NavigationStack {
                LegalDocumentView(document: document)
                    .toolbar { ToolbarItem(placement: .confirmationAction) { Button("닫기") { legalDocument = nil } } }
            }
        }
        .task { async let boot: Void = session.prepare(); await intro.start(); await boot }
        .onChange(of: scenePhase) { _, phase in
            if phase == .active { session.resumeLocationSharingIfNeeded() }
        }
        .onOpenURL { url in
            if !session.receiveInvite(url.absoluteString) { session.errorMessage = "초대 링크를 확인해 주세요." }
        }
    }
}
#Preview { RootView() }
