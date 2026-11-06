import SwiftUI
import Observation

@MainActor
@Observable final class IntroViewModel {
    enum Phase { case words, logo }
    private(set) var isFinished = false
    private(set) var phase: Phase = .words
    private(set) var elapsed: TimeInterval = 0
    @ObservationIgnored private var isActive = true
    @ObservationIgnored private(set) var isWordmarkReady = false

    func setActive(_ active: Bool) { isActive = active }
    func stop() { isActive = false }
    func wordmarkDidDraw() { isWordmarkReady = true }

    private func wait(seconds: TimeInterval, updateWords: Bool = false) async throws {
        var spent: TimeInterval = 0
        var previous = ContinuousClock.now
        while spent < seconds {
            try await Task.sleep(for: .milliseconds(16))
            let now = ContinuousClock.now
            let duration = previous.duration(to: now).components
            previous = now
            if isActive {
                spent = min(seconds, spent + Double(duration.seconds) + Double(duration.attoseconds) / 1e18)
                if updateWords { elapsed = spent }
            }
        }
    }

    func start(reduceMotion: Bool) async {
        guard !isFinished else { return }
        phase = .words
        elapsed = 0
        do {
            if !reduceMotion {
                while !isWordmarkReady { try await Task.sleep(for: .milliseconds(16)) }
                try await wait(seconds: IntroTimeline.duration, updateWords: true)
            }
            try Task.checkCancellation()
            phase = .logo
            try await wait(seconds: 1)
            try Task.checkCancellation()
            withAnimation(reduceMotion ? nil : .easeInOut(duration: 0.3)) { isFinished = true }
        } catch { return }
    }
}
