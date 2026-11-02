import SwiftUI

struct CreateRoomView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var model = CreateRoomViewModel()
    let onCreate: (Room) -> Void
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 28) {
                    FormHeading(title: "새로운 만남의 시작", subtitle: "모임을 만들고 함께할 사람들을 초대하세요.")
                    InputField(title: "모임 이름", placeholder: "예: 주말 서울 산책", text: $model.title)
                    InputField(title: "내 이름 · 방장", placeholder: "멤버들에게 보일 이름", text: $model.name)
                }.padding(24)
            }.scrollDismissesKeyboard(.interactively)
                .safeAreaInset(edge: .bottom) {
                    PrimaryButton(title: "모임 만들기", disabled: !model.canCreate) { onCreate(model.makeRoom()) }.padding(24).background(AppStyle.background)
                }
                .background(AppStyle.background)
                .toolbarBackground(AppStyle.background, for: .navigationBar)
                .navigationTitle("모임 만들기").navigationBarTitleDisplayMode(.inline)
                .toolbar { ToolbarItem(placement: .cancellationAction) { Button("닫기") { dismiss() } } }
        }
    }
}
#Preview { CreateRoomView { _ in } }
