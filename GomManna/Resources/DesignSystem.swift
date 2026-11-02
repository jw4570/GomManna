import SwiftUI
import UIKit

enum AppStyle {
    // 코코아 브라운과 크림. 다크 모드에서는 밝은 카라멜로 대비를 유지합니다.
    static let accent = adaptive(light: UIColor(red: 0.43, green: 0.29, blue: 0.21, alpha: 1), dark: UIColor(red: 0.84, green: 0.66, blue: 0.48, alpha: 1))
    static let onAccent = adaptive(light: .white, dark: UIColor(red: 0.18, green: 0.12, blue: 0.08, alpha: 1))
    static let accentSoft = adaptive(light: UIColor(red: 0.94, green: 0.88, blue: 0.79, alpha: 1), dark: UIColor(red: 0.24, green: 0.19, blue: 0.15, alpha: 1))
    static let background = adaptive(light: UIColor(red: 0.99, green: 0.98, blue: 0.95, alpha: 1), dark: UIColor(red: 0.10, green: 0.08, blue: 0.07, alpha: 1))
    static let field = adaptive(light: UIColor(red: 0.96, green: 0.93, blue: 0.88, alpha: 1), dark: UIColor(red: 0.17, green: 0.14, blue: 0.12, alpha: 1))
    private static func adaptive(light: UIColor, dark: UIColor) -> Color {
        Color(uiColor: UIColor { traits in traits.userInterfaceStyle == .dark ? dark : light })
    }
}
