import SwiftUI

/// 투명 글자만 직접 그려 비디오의 불투명 합성·포스터 잔상을 피합니다.
struct IntroAnimationView: View {
    let model: IntroViewModel

    var body: some View {
        let timeline = IntroTimeline(elapsed: model.elapsed)
        let needsFirstDrawSignal = !model.isWordmarkReady
        Canvas { context, size in
            if needsFirstDrawSignal, size.width > 0, size.height > 0 {
                Task { @MainActor in model.wordmarkDidDraw() }
            }
            context.scaleBy(x: size.width / 1080, y: size.height / 720)
            func draw(_ name: String, x: Double, y: Double, width: Double, height: Double,
                      opacity: Double, in target: inout GraphicsContext) {
                guard opacity > 0 else { return }
                var layer = target
                layer.opacity = opacity
                var image = layer.resolve(Image(name))
                image.shading = .color(AppStyle.accent)
                layer.draw(image, in: CGRect(x: x, y: y, width: width, height: height))
            }
            draw("IntroOur", x: 268, y: 324, width: 154.5, height: 86,
                 opacity: timeline.outerOpacity, in: &context)
            draw("IntroYo", x: 734, y: 329, width: 78, height: 70,
                 opacity: timeline.outerOpacity, in: &context)
            // 공통 부분인 고가 중간에 흐려지지 않도록 더해서 합성합니다.
            context.drawLayer { glyph in
                glyph.blendMode = .plusLighter
                draw("IntroSoon", x: timeline.soonX + 2.5, y: 326.5, width: 78, height: 82.5,
                     opacity: 1 - timeline.morph, in: &glyph)
                draw("IntroBear", x: timeline.soonX + 2.5, y: 327, width: 78, height: 82,
                     opacity: timeline.morph, in: &glyph)
            }
            draw("IntroMeet", x: timeline.meetX + 5, y: 324, width: 160, height: 86,
                 opacity: 1, in: &context)
        }
        .accessibilityHidden(true)
    }
}
