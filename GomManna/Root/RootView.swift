import SwiftUI

struct RootView: View {
    @State private var intro = IntroViewModel()
    var body: some View {
        ZStack {
            if intro.isFinished { HomeView().transition(.opacity) }
            else { IntroView().transition(.opacity) }
        }.task { await intro.start() }
    }
}
#Preview { RootView() }
