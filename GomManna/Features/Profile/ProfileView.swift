import SwiftUI

struct ProfileView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var model = ProfileViewModel.shared
    private let columns = Array(repeating: GridItem(.flexible(), spacing: 14), count: 3)

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 24) {
                    MemberAvatar(character: model.character, size: 88).padding(.top, 16)
                    VStack(spacing: 8) {
                        Text("나를 닮은 동물").font(.title2.bold())
                        Text("함께할 캐릭터를 골라주세요.").font(.subheadline).foregroundStyle(.secondary)
                    }
                    LazyVGrid(columns: columns, spacing: 14) {
                        ForEach(AnimalCharacter.allCases) { animal in
                            Button { model.select(animal) } label: {
                                VStack(spacing: 10) {
                                    MemberAvatar(character: animal, size: 52)
                                    Text(animal.name).font(.subheadline.weight(.medium))
                                }
                                .frame(maxWidth: .infinity).padding(.vertical, 18)
                                .background(AppStyle.field, in: RoundedRectangle(cornerRadius: 20))
                                .overlay {
                                    RoundedRectangle(cornerRadius: 20)
                                        .stroke(model.character == animal ? AppStyle.accent : .clear, lineWidth: 2)
                                }
                                .overlay(alignment: .topTrailing) {
                                    if model.character == animal {
                                        Image(systemName: "checkmark.circle.fill")
                                            .foregroundStyle(AppStyle.onAccent, AppStyle.accent).padding(8)
                                    }
                                }
                            }.buttonStyle(.plain)
                                .accessibilityLabel(animal.name)
                                .accessibilityAddTraits(model.character == animal ? .isSelected : [])
                        }
                    }
                }.padding(24)
            }
            .background(AppStyle.background)
            .navigationTitle("내 프로필").navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("완료") { dismiss() } } }
        }.tint(AppStyle.accent)
    }
}
#Preview { ProfileView() }
