import SwiftUI

struct IntroView: View {
    let model: IntroViewModel
    @Environment(\.scenePhase) private var scenePhase
    @Environment(\.accessibilityReduceMotion) private var reduceMotion


    var body: some View {
        ZStack {
            AppStyle.background.ignoresSafeArea()
            if model.phase == .words {
                IntroAnimationView(model: model)
                .aspectRatio(1.5, contentMode: .fit)
                .frame(maxWidth: 600)
                .padding(.horizontal, 12)
                .accessibilityElement(children: .ignore)
                .accessibilityLabel("우리 곧 만나요. 곰만나")
                .transition(.opacity)
            } else {
                BrandMark(size: 128).transition(.opacity)
            }
        }
        .animation(reduceMotion ? nil : .easeInOut(duration: 0.25), value: model.phase)
        .task {
            model.setActive(scenePhase == .active)
            await model.start(reduceMotion: reduceMotion)
        }
        .onChange(of: scenePhase) { _, phase in model.setActive(phase == .active) }
        .onDisappear { model.stop() }
    }
}
#Preview { IntroView(model: IntroViewModel()) }
