import SwiftUI

struct BrandMark: View {
    var size: CGFloat = 64
    var body: some View {
        ZStack {
            HStack(spacing: size * 0.36) {
                Circle().frame(width: size * 0.28, height: size * 0.28)
                Circle().frame(width: size * 0.28, height: size * 0.28)
            }.offset(y: -size * 0.30)
            Ellipse().frame(width: size * 0.91, height: size * 0.82)
            HStack(spacing: size * 0.28) {
                Circle().frame(width: size * 0.065, height: size * 0.065)
                Circle().frame(width: size * 0.065, height: size * 0.065)
            }.foregroundStyle(AppStyle.background).offset(y: -size * 0.06)
            Ellipse().fill(AppStyle.accentSoft).frame(width: size * 0.36, height: size * 0.25).offset(y: size * 0.15)
            Ellipse().frame(width: size * 0.12, height: size * 0.08).offset(y: size * 0.12)
        }
        .foregroundStyle(AppStyle.accent)
        .frame(width: size, height: size)
        .accessibilityLabel("곰만나 로고")
    }
}

struct PrimaryButton: View {
    let title: String
    var secondary = false
    var disabled = false
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            Text(title).font(.headline).frame(maxWidth: .infinity).padding(.vertical, 18)
                .foregroundStyle(secondary ? AppStyle.accent : AppStyle.onAccent)
                .background(secondary ? AppStyle.accentSoft : AppStyle.accent, in: RoundedRectangle(cornerRadius: 19))
        }.buttonStyle(.plain).disabled(disabled).opacity(disabled ? 0.4 : 1)
    }
}

struct InputField: View {
    let title: String
    let placeholder: String
    @Binding var text: String
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title).font(.subheadline.weight(.semibold))
            TextField(placeholder, text: $text)
                .padding(18).background(AppStyle.field, in: RoundedRectangle(cornerRadius: 16))
                .autocorrectionDisabled()
        }
    }
}

struct FormHeading: View {
    let title: String
    let subtitle: String
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title).font(.system(size: 30, weight: .bold))
            Text(subtitle).font(.subheadline).foregroundStyle(.secondary)
        }.frame(maxWidth: .infinity, alignment: .leading).padding(.vertical, 24)
    }
}

struct MemberAvatar: View {
    let character: AnimalCharacter
    var size: CGFloat = 44
    var body: some View {
        Text(character.emoji).font(.system(size: size * 0.65))
            .frame(width: size, height: size)
            .background(AppStyle.accentSoft, in: Circle())
            .accessibilityLabel("\(character.name) 캐릭터")
    }
}
