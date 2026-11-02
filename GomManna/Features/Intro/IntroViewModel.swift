import SwiftUI
import Observation

@Observable final class IntroViewModel {
    var isFinished = false
    func start() async {
        do { try await Task.sleep(for: .seconds(2)) } catch { return }
        withAnimation(.easeInOut(duration: 0.45)) { isFinished = true }
    }
}
