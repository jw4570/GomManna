import SwiftUI

struct ChatView: View {
    let room: Room
    @Bindable var model: ChatViewModel
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            ScrollViewReader { proxy in
                ScrollView {
                    if model.messages.isEmpty {
                        ContentUnavailableView("어디쯤 왔나요?", systemImage: "bubble.left.and.bubble.right", description: Text("함께하는 멤버에게 인사를 건네보세요."))
                            .padding(.top, 70)
                    } else {
                        LazyVStack(alignment: .trailing, spacing: 18) {
                            ForEach(model.messages) { message in
                                HStack(alignment: .bottom, spacing: 8) {
                                    Spacer(minLength: 48)
                                    Text(message.sentAt, style: .time)
                                        .font(.caption2).foregroundStyle(.secondary)
                                    Text(message.text)
                                        .padding(.horizontal, 16).padding(.vertical, 12)
                                        .foregroundStyle(AppStyle.onAccent)
                                        .background(AppStyle.accent, in: RoundedRectangle(cornerRadius: 20))
                                        .textSelection(.enabled)
                                }.id(message.id)
                            }
                        }.padding(20)
                    }
                }
                .scrollDismissesKeyboard(.interactively)
                .onChange(of: model.messages.count) { _, _ in
                    if let id = model.messages.last?.id {
                        withAnimation { proxy.scrollTo(id, anchor: .bottom) }
                    }
                }
                .defaultScrollAnchor(.bottom)
            }
            .safeAreaInset(edge: .bottom, spacing: 0) {
                VStack(spacing: 10) {
                    Text("화면 미리보기 · 메시지는 다른 멤버에게 전송되지 않아요")
                        .font(.caption2).foregroundStyle(.secondary)
                    HStack(alignment: .bottom, spacing: 12) {
                        TextField("메시지 입력", text: $model.draft, axis: .vertical)
                            .lineLimit(1...5).padding(14)
                            .background(AppStyle.field, in: RoundedRectangle(cornerRadius: 22))
                        Button { model.send() } label: {
                            Image(systemName: "arrow.up").font(.headline)
                                .foregroundStyle(AppStyle.onAccent)
                                .frame(width: 46, height: 46)
                                .background(AppStyle.accent, in: Circle())
                        }.disabled(!model.canSend).opacity(model.canSend ? 1 : 0.4)
                            .accessibilityLabel("메시지 보내기")
                    }
                }.padding(.horizontal, 20).padding(.vertical, 12).background(AppStyle.background)
            }
            .background(AppStyle.background)
            .navigationTitle("모임 대화").navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .principal) {
                    VStack(spacing: 3) {
                        Text("모임 대화").font(.headline)
                        Text(room.title).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                    }
                }
                ToolbarItem(placement: .confirmationAction) { Button("닫기") { dismiss() } }
            }
        }.tint(AppStyle.accent)
    }
}
#Preview { ChatView(room: .sample, model: ChatViewModel()) }
