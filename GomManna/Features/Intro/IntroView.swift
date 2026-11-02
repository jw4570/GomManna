import SwiftUI

struct IntroView: View {
    var body: some View {
        ZStack {
            AppStyle.background.ignoresSafeArea()
            VStack(spacing: 20) {
                BrandMark(size: 86)
                Text("곰만나").font(.system(size: 38, weight: .bold, design: .rounded))
                Text("함께라면, 헤매지 않도록").font(.subheadline).foregroundStyle(.secondary)
            }
        }
    }
}
#Preview { IntroView() }
