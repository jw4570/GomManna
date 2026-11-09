import SwiftUI
import UIKit

/// 설정 변경을 View에서 관찰하고 SwiftUI와 UIKit의 실제 윈도우 테마를 함께 맞춥니다.
struct AppAppearanceModifier: ViewModifier {
    @AppStorage("GomManna.appearance") private var appearance = "system"

    private var colorScheme: ColorScheme? {
        switch appearance {
        case "light": .light
        case "dark": .dark
        default: nil
        }
    }

    private var interfaceStyle: UIUserInterfaceStyle {
        switch appearance {
        case "light": .light
        case "dark": .dark
        default: .unspecified
        }
    }

    func body(content: Content) -> some View {
        content
            .preferredColorScheme(colorScheme)
            .background { WindowAppearanceBridge(style: interfaceStyle).allowsHitTesting(false) }
    }
}

private struct WindowAppearanceBridge: UIViewRepresentable {
    let style: UIUserInterfaceStyle

    func makeUIView(context: Context) -> StyleView {
        let view = StyleView()
        view.isUserInteractionEnabled = false
        view.style = style
        return view
    }

    func updateUIView(_ view: StyleView, context: Context) {
        view.style = style
        view.applyStyle()
    }

    final class StyleView: UIView {
        var style: UIUserInterfaceStyle = .unspecified

        override func didMoveToWindow() {
            super.didMoveToWindow()
            applyStyle()
        }

        func applyStyle() {
            DispatchQueue.main.async { [weak self] in
                guard let self, let window = self.window,
                      window.overrideUserInterfaceStyle != self.style else { return }
                window.overrideUserInterfaceStyle = self.style
            }
        }
    }
}
