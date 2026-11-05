import Foundation
import Security

struct TokenStore {
    private let service = Bundle.main.bundleIdentifier ?? "GomManna"
    private let account = "guest-access-token"

    enum StoreError: Error {
        case keychain(OSStatus)
        case invalidData
    }

    private var query: [String: Any] {
        [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account
        ]
    }

    func save(_ token: String) throws {
        let data = Data(token.utf8)

        let status = SecItemUpdate(
            query as CFDictionary,
            [kSecValueData as String: data] as CFDictionary
        )

        if status == errSecItemNotFound {
            var item = query
            item[kSecValueData as String] = data
            item[kSecAttrAccessible as String] =
                kSecAttrAccessibleWhenUnlockedThisDeviceOnly

            let addStatus = SecItemAdd(item as CFDictionary, nil)

            guard addStatus == errSecSuccess else {
                throw StoreError.keychain(addStatus)
            }
        } else if status != errSecSuccess {
            throw StoreError.keychain(status)
        }
    }


    func read() throws -> String? {
        var item = query
        item[kSecReturnData as String] = true
        item[kSecMatchLimit as String] = kSecMatchLimitOne

        var result: CFTypeRef?
        let status = SecItemCopyMatching(item as CFDictionary, &result)

        if status == errSecItemNotFound {
            return nil
        }

        guard status == errSecSuccess else {
            throw StoreError.keychain(status)
        }

        guard let data = result as? Data,
              let token = String(data: data, encoding: .utf8) else {
            throw StoreError.invalidData
        }

        return token
    }

    func delete() throws {
        let status = SecItemDelete(query as CFDictionary)

        guard status == errSecSuccess || status == errSecItemNotFound else {
            throw StoreError.keychain(status)
        }
    }
}
