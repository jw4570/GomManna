import Foundation
import CoreImage

/// 실 카메라로 읽을 수 있는 QR SVG를 생성합니다.
enum MockQRCode {
    static func svg(_ payload: String) throws -> String {
        guard let filter = CIFilter(name: "CIQRCodeGenerator") else { throw URLError(.cannotDecodeContentData) }
        filter.setValue(Data(payload.utf8), forKey: "inputMessage")
        filter.setValue("M", forKey: "inputCorrectionLevel")
        guard let image = filter.outputImage else { throw URLError(.cannotDecodeContentData) }
        let width = Int(image.extent.width)
        let height = Int(image.extent.height)
        var pixels = [UInt8](repeating: 0, count: width * height * 4)
        pixels.withUnsafeMutableBytes { buffer in
            CIContext().render(image, toBitmap: buffer.baseAddress!, rowBytes: width * 4,
                               bounds: image.extent, format: .RGBA8, colorSpace: CGColorSpaceCreateDeviceRGB())
        }
        var path = ""
        for y in 0..<height {
            for x in 0..<width where pixels[(y * width + x) * 4] < 128 {
                path += "M\(x + 4) \(height - 1 - y + 4)h1v1h-1z"
            }
        }
        return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 \(width + 8) \(height + 8)\" shape-rendering=\"crispEdges\"><path fill=\"white\" d=\"M0 0h\(width + 8)v\(height + 8)H0z\"/><path fill=\"black\" d=\"\(path)\"/></svg>"
    }
}
