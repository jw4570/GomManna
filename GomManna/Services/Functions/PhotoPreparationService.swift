import UIKit

struct PhotoPreparationService {
    /// 렌더링으로 EXIF 방향을 적용하고 최대 변을 2048px로 줄입니다.
    func jpeg(from data: Data) throws -> Data {
        guard let image = UIImage(data: data), image.size.width > 0, image.size.height > 0 else {
            throw APIRequestError(statusCode: 400, message: "사진을 읽을 수 없어요.")
        }
        let ratio = min(1, 2048 / max(image.size.width, image.size.height))
        let size = CGSize(width: image.size.width * ratio, height: image.size.height * ratio)
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        format.opaque = true
        let normalized = UIGraphicsImageRenderer(size: size, format: format).image { _ in
            UIColor.white.setFill()
            UIRectFill(CGRect(origin: .zero, size: size))
            image.draw(in: CGRect(origin: .zero, size: size))
        }
        for quality in [0.85, 0.65, 0.45] {
            if let result = normalized.jpegData(compressionQuality: quality), result.count <= 5 * 1024 * 1024 { return result }
        }
        throw APIRequestError(statusCode: 413, message: "사진 크기를 줄여 다시 시도해 주세요.", code: "PHOTO_TOO_LARGE")
    }
}
