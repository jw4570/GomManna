import Foundation
import ImageIO
import CoreGraphics

/// 이미지 압축 해제와 축소는 UI 실행 영역 밖에서 수행합니다.
actor MemberPhotoDecoder {
    static let shared = MemberPhotoDecoder()

    func decode(_ data: Data, maximumPixelSize: Int) throws -> CGImage {
        guard let source = CGImageSourceCreateWithData(data as CFData, [
            kCGImageSourceShouldCache: false
        ] as CFDictionary),
        let image = CGImageSourceCreateThumbnailAtIndex(source, 0, [
            kCGImageSourceCreateThumbnailFromImageAlways: true,
            kCGImageSourceCreateThumbnailWithTransform: true,
            kCGImageSourceThumbnailMaxPixelSize: maximumPixelSize,
            kCGImageSourceShouldCacheImmediately: true
        ] as CFDictionary) else {
            throw URLError(.cannotDecodeContentData)
        }
        return image
    }
}
