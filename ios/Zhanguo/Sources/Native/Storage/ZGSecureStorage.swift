import Foundation
import Security
import CoreFoundation

/// Only device capabilities and the existing allowlisted lease fields belong
/// here. Login passwords and ordinary game bearer tokens are never persisted.
final class ZGSecureStorage {
    private let queue = DispatchQueue(label: "cn.zhanguo.secure-storage")
    private let service: String

    init(service: String = (Bundle.main.bundleIdentifier ?? "cn.zhanguo.ios") + ".capabilities") {
        self.service = service
    }

    func request(operation: String, value: String?, completion: @escaping (Result<Any, Error>) -> Void) {
        queue.async {
            let result: Result<Any, Error>
            do {
                switch operation {
                case "device": result = .success(try self.device())
                case "read": result = .success(try self.read("lease") ?? "")
                case "record":
                    guard let value else { throw StorageError.invalid }
                    try self.write("lease", value: Self.sanitizeLease(value))
                    result = .success(true)
                case "forget": try self.remove("lease"); result = .success(true)
                default: throw StorageError.invalid
                }
            } catch { result = .failure(error) }
            DispatchQueue.main.async { completion(result) }
        }
    }

    private func query(_ account: String) -> [String: Any] {
        [kSecClass as String: kSecClassGenericPassword,
         kSecAttrService as String: service, kSecAttrAccount as String: account,
         kSecAttrSynchronizable as String: false]
    }

    private func read(_ account: String) throws -> String? {
        var attributes = query(account)
        attributes[kSecReturnData as String] = true
        attributes[kSecMatchLimit as String] = kSecMatchLimitOne
        var item: CFTypeRef?
        let status = SecItemCopyMatching(attributes as CFDictionary, &item)
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess, let data = item as? Data,
              let value = String(data: data, encoding: .utf8) else { throw StorageError.unavailable }
        return value
    }

    private func write(_ account: String, value: String) throws {
        let data = Data(value.utf8)
        let updates: [String: Any] = [kSecValueData as String: data,
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly]
        let status = SecItemUpdate(query(account) as CFDictionary, updates as CFDictionary)
        if status == errSecItemNotFound {
            var attributes = query(account)
            for (key, value) in updates { attributes[key] = value }
            guard SecItemAdd(attributes as CFDictionary, nil) == errSecSuccess else { throw StorageError.unavailable }
        } else if status != errSecSuccess { throw StorageError.unavailable }
    }

    private func remove(_ account: String) throws {
        let status = SecItemDelete(query(account) as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else { throw StorageError.unavailable }
    }

    private func device() throws -> String {
        if let saved = try read("device") {
            guard let data = saved.data(using: .utf8),
                  let value = try JSONSerialization.jsonObject(with: data) as? [String: String],
                  ["deviceId", "deviceSecret"].allSatisfy({ value[$0]?.range(of: "^[a-f0-9]{64}$", options: .regularExpression) != nil })
            else { throw StorageError.unavailable }
            return saved
        }
        let object = ["deviceId": try Self.randomHex(), "deviceSecret": try Self.randomHex()]
        let data = try JSONSerialization.data(withJSONObject: object, options: .sortedKeys)
        guard let value = String(data: data, encoding: .utf8) else { throw StorageError.unavailable }
        try write("device", value: value)
        return value
    }

    private static func randomHex() throws -> String {
        var bytes = [UInt8](repeating: 0, count: 32)
        let status = bytes.withUnsafeMutableBytes { SecRandomCopyBytes(kSecRandomDefault, $0.count, $0.baseAddress!) }
        guard status == errSecSuccess else { throw StorageError.unavailable }
        return bytes.map { String(format: "%02x", $0) }.joined()
    }

    static func sanitizeLease(_ text: String) throws -> String {
        guard text.utf8.count <= 8192, let data = text.data(using: .utf8),
              let source = try JSONSerialization.jsonObject(with: data) as? [String: Any] else { throw StorageError.invalid }
        var safe: [String: Any] = [:]
        func copyText(_ key: String, limit: Int) {
            if let value = source[key] as? String, value.utf16.count <= limit { safe[key] = value }
        }
        for (key, limit) in [("account",31),("leaseId",64),("state",32),("startKey",128),
                              ("resumeKey",128),("stopKey",128),("message",2048)] { copyText(key, limit: limit) }
        for key in ["zoneId", "characterId", "startedAt"] {
            if let value = source[key] as? NSNumber, CFGetTypeID(value) != CFBooleanGetTypeID(),
               value.doubleValue.isFinite, (0...9007199254740991).contains(value.doubleValue) { safe[key] = value }
        }
        if let value = source["autoRenew"] as? NSNumber, CFGetTypeID(value) == CFBooleanGetTypeID() { safe["autoRenew"] = value }
        for (key, textFields, numberFields) in [("activity", ["kind":32,"name":64], ["mapId"]),
            ("renewCourse", ["mode":32,"text":256], ["eventId","gold","hours","statusId"])] {
            if let nested = source[key] as? [String: Any] {
                var filtered: [String: Any] = [:]
                for (field, limit) in textFields {
                    if let value = nested[field] as? String, value.utf16.count <= limit { filtered[field] = value }
                }
                for field in numberFields {
                    if let value = nested[field] as? NSNumber, CFGetTypeID(value) != CFBooleanGetTypeID(),
                       value.doubleValue.isFinite, (0...9007199254740991).contains(value.doubleValue) { filtered[field] = value }
                }
                safe[key] = filtered
            }
        }
        guard safe["account"] != nil, safe["startKey"] != nil else { throw StorageError.invalid }
        let result = try JSONSerialization.data(withJSONObject: safe, options: .sortedKeys)
        guard let value = String(data: result, encoding: .utf8) else { throw StorageError.invalid }
        return value
    }

    enum StorageError: LocalizedError {
        case invalid, unavailable
        var errorDescription: String? { "本机保存未能完成，请保持在线后重试。" }
    }
}
