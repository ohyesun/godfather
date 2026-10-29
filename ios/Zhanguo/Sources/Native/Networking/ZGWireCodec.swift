import Foundation
import CoreFoundation

/// Only launcher/TLS envelope framing lives here. Game RC5/DES/zlib and
/// fragmented 8-byte game crypto blocks stay in the unchanged JavaScript codec.
enum ZGWireCodec {
    static let launcherVersion = "1.0.0.15"
    static let gb18030 = String.Encoding(rawValue: CFStringConvertEncodingToNSStringEncoding(
        CFStringEncoding(CFStringEncodings.GB_18030_2000.rawValue)))
    static func appendLE(_ value: UInt64, bytes: Int, to data: inout Data) {
        for n in 0..<bytes { data.append(UInt8(truncatingIfNeeded: value >> (8 * n))) }
    }
    static func appendBE(_ value: UInt64, bytes: Int, to data: inout Data) {
        for n in (0..<bytes).reversed() { data.append(UInt8(truncatingIfNeeded: value >> (8 * n))) }
    }
    static func integer(_ data: Data, _ offset: Int, _ count: Int, bigEndian: Bool = false) throws -> UInt64 {
        guard offset >= 0, count > 0, count <= 8, offset <= data.count - count else {
            throw ZGNativeNetworkFailure.invalid("原生协议长度无效")
        }
        var value: UInt64 = 0
        for n in 0..<count {
            let byte = UInt64(data[data.startIndex + offset + n])
            value |= byte << (8 * (bigEndian ? count - 1 - n : n))
        }
        return value
    }
    static func decodeHex(_ text: String) throws -> Data {
        let bytes = Array(text.utf8)
        guard bytes.count % 2 == 0, bytes.count <= ZGNetworkLimits.rawBytes * 2 else {
            throw ZGNativeNetworkFailure.invalid("发送数据大小无效")
        }
        func nibble(_ b: UInt8) throws -> UInt8 {
            switch b { case 48...57: return b - 48; case 65...70: return b - 55; case 97...102: return b - 87
            default: throw ZGNativeNetworkFailure.invalid("发送数据格式无效") }
        }
        var result = Data(); result.reserveCapacity(bytes.count / 2)
        for n in stride(from: 0, to: bytes.count, by: 2) {
            result.append(try nibble(bytes[n]) << 4 | nibble(bytes[n + 1]))
        }
        return result
    }
    static func hex(_ data: Data) -> String {
        let digits = Array("0123456789abcdef".utf8)
        var output = [UInt8](); output.reserveCapacity(data.count * 2)
        for b in data { output.append(digits[Int(b >> 4)]); output.append(digits[Int(b & 15)]) }
        return String(decoding: output, as: UTF8.self)
    }
    static func launcherRequest(command: UInt8, payload: Data) throws -> Data {
        guard payload.count <= ZGNetworkLimits.launcherBytes else { throw ZGNativeNetworkFailure.invalid("登录器请求过大") }
        var result = Data([1, command, 0, 0]); appendLE(UInt64(payload.count), bytes: 4, to: &result)
        appendLE(0, bytes: 4, to: &result); result.append(payload); return result
    }
    static func versionPayload(authentication: Data) throws -> Data {
        guard !authentication.isEmpty, authentication.count <= 256,
              authentication.allSatisfy({ $0 > 0 && $0 < 128 }) else {
            throw ZGNativeNetworkFailure.invalid("登录器认证配置缺失或无效")
        }
        var data = Data((launcherVersion + "|").utf8); data.append(authentication); return data
    }
    static func filePayload() -> Data {
        let name = Data("LauncherConfig.txt".utf8); var result = Data()
        appendLE(0, bytes: 8, to: &result); appendLE(0, bytes: 4, to: &result)
        appendLE(UInt64(name.count), bytes: 4, to: &result); result.append(name)
        // Original Java allocates 20+name.length but writes 16+name.length.
        // The trailing four zero bytes are part of its actual wire request.
        appendLE(0, bytes: 4, to: &result); return result
    }
    struct LauncherHeader { let status: Int; let payloadBytes: Int; let fileBytes: Int }
    static func launcherHeader(_ data: Data) throws -> LauncherHeader {
        guard data.count == 20, data.first == 1 else { throw ZGNativeNetworkFailure.invalid("登录器响应格式无效") }
        let payload = try integer(data, 4, 4), file = try integer(data, 8, 8)
        guard payload <= UInt64(ZGNetworkLimits.launcherBytes), file <= UInt64(ZGNetworkLimits.launcherBytes) else {
            throw ZGNativeNetworkFailure.invalid("登录器响应过大")
        }
        return LauncherHeader(status: Int(data[data.startIndex + 1]), payloadBytes: Int(payload), fileBytes: Int(file))
    }
    static func validGBK(_ data: Data) -> Bool {
        let bytes = Array(data); var n = 0
        while n < bytes.count {
            if bytes[n] <= 0x7f || bytes[n] == 0x80 { n += 1; continue }
            guard (0x81...0xfe).contains(bytes[n]), n + 1 < bytes.count,
                  (0x40...0xfe).contains(bytes[n + 1]), bytes[n + 1] != 0x7f else { return false }
            n += 2
        }
        return true
    }
    static func decodeLauncherText(_ data: Data) throws -> String {
        if let value = String(data: data, encoding: .utf8) { return value }
        guard validGBK(data), let value = String(data: data, encoding: gb18030) else {
            throw ZGNativeNetworkFailure.invalid("区服列表文字编码无效")
        }
        return value
    }
    static func encodeGBK(_ text: String) throws -> Data {
        guard let data = text.data(using: gb18030, allowLossyConversion: false), validGBK(data),
              String(data: data, encoding: gb18030) == text else {
            throw ZGNativeNetworkFailure.invalid("注册信息含区服不支持的字符")
        }
        return data
    }
    static func registerPayload(_ input: [String: Any]) throws -> Data {
        let account = (input["account"] as? String ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        let password = input["password"] as? String ?? ""
        let qq = (input["qq"] as? String ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        let zone = (input["zone"] as? String ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        guard account.range(of: "^[A-Za-z][A-Za-z0-9_]{4,46}[A-Za-z0-9]$", options: .regularExpression) != nil,
              !password.isEmpty, password.utf16.count <= 128, qq.utf16.count <= 128, zone.utf16.count <= 128 else {
            throw ZGNativeNetworkFailure.invalid("注册信息格式无效")
        }
        let fields = try [account, password, qq + (zone.isEmpty ? "" : "&zone=" + zone)].map(encodeGBK)
        guard fields.allSatisfy({ $0.count <= 256 }) else { throw ZGNativeNetworkFailure.invalid("注册信息过长") }
        var result = Data()
        for value in fields { appendLE(UInt64(value.count), bytes: 4, to: &result) }
        for value in fields { result.append(value) }; return result
    }
    static func relayFrame(id: UInt64, payload: Data) throws -> Data {
        guard payload.count <= ZGNetworkLimits.relayRequestBytes else { throw ZGNativeNetworkFailure.invalid("网关请求过大") }
        var result = Data("ZGTP".utf8); result.append(contentsOf: [1, 0]); appendBE(id, bytes: 8, to: &result)
        appendBE(UInt64(payload.count), bytes: 4, to: &result); result.append(payload); return result
    }
    static func relayLength(_ data: Data, id: UInt64) throws -> Int {
        guard data.count == 18, data.prefix(6) == Data([90, 71, 84, 80, 1, 0]),
              try integer(data, 6, 8, bigEndian: true) == id else {
            throw ZGNativeNetworkFailure.invalid("网关响应编号或格式无效")
        }
        let length = try integer(data, 14, 4, bigEndian: true)
        guard length <= UInt64(ZGNetworkLimits.relayResponseBytes) else { throw ZGNativeNetworkFailure.invalid("网关响应过大") }
        return Int(length)
    }
}
