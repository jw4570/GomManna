import Foundation

enum IntroChecks {
    @MainActor static func run() async throws {
        for time in [0.0, 0.5, 1.0] {
            let frame = IntroTimeline(elapsed: time)
            precondition(frame.outerOpacity == 1 && frame.morph == 0 && frame.join == 0)
        }
        let removed = IntroTimeline(elapsed: 1 + 1 / 1.2)
        precondition(removed.outerOpacity < 0.00001 && removed.morph < 0.00001 && removed.join == 0)
        let changed = IntroTimeline(elapsed: 1 + 1.65 / 1.2)
        precondition(changed.morph > 0.99999 && changed.join < 0.00001)
        let final = IntroTimeline(elapsed: IntroTimeline.duration)
        precondition(final.join == 1 && final.outerOpacity == 0)
        precondition(final.meetX - final.soonX == 83)
        let model = IntroViewModel()
        let run = Task { await model.start(reduceMotion: false) }
        try await Task.sleep(for: .milliseconds(50))
        precondition(model.elapsed == 0 && model.phase == .words)
        model.wordmarkDidDraw()
        try await Task.sleep(for: .milliseconds(100))
        precondition(model.elapsed > 0 && !model.isFinished)
        run.cancel()
        await run.value
        precondition(!model.isFinished)
        print("PASS: intro initial hold, remove/morph/join order, first-draw gating and cancellation")
    }
}
