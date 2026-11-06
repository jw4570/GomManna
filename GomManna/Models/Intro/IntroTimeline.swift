import Foundation

/// 승인한 영상과 동일한 96pt 글자 배치·변환 타이밍입니다.
struct IntroTimeline {
    static let duration: TimeInterval = 4.2
    let elapsed: TimeInterval
    private var time: Double { max(0, elapsed - 1) * 1.2 }
    private func progress(_ start: Double, _ end: Double) -> Double {
        let value = min(1, max(0, (time - start) / (end - start)))
        return value * value * (3 - 2 * value)
    }
    var outerOpacity: Double { pow(1 - progress(0.15, 1), 2) }
    var morph: Double { progress(1, 1.65) }
    var join: Double { progress(1.65, 2.55) }
    var soonX: Double { 457 - 41.5 * join }
    var meetX: Double { soonX + 83 + 25.5 * (1 - join) }
}
