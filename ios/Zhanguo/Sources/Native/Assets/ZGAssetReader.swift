import Foundation
import zlib

/// A per-request FileHandle and z_stream permit concurrent model/texture
/// loads without sharing a seek cursor. Neither the complete PAK nor a whole
/// decoded model is held in this native layer's memory.
enum ZGAssetReader {
    static let chunkSize = 64 * 1024

    static func stream(_ source: ZGAssetSource, range: Range<Int64>,
                       cancelled: () -> Bool, emit: (Data) -> Bool) throws {
        guard range.lowerBound >= 0, range.upperBound <= source.size else { throw ZGAssetError.invalidPath }
        if range.isEmpty { return }
        if let remoteId = source.remoteId {
            // 最小包：这段不在本地 pak 里。先保证解码后的本地副本存在
            // （缓存命中则完全不连网），再按普通本地资源走下面的分支。
            let cached: URL
            do {
                cached = try ZGMicroFetcher.shared.resolve(
                    remoteId: remoteId, storedSize: source.storedSize, decodedSize: source.size,
                    codec: source.codec, sha256: source.sha256, cancelled: cancelled)
            } catch let failure as ZGMicroFailure {
                if case .cancelled = failure { throw ZGAssetError.cancelled }
                throw failure
            }
            let resolved = ZGAssetSource(fileURL: cached, offset: 0, storedSize: source.size,
                                         size: source.size, codec: 0, sha256: source.sha256,
                                         remoteId: nil)
            return try stream(resolved, range: range, cancelled: cancelled, emit: emit)
        }
        guard let fileURL = source.fileURL else { throw ZGAssetError.missingFile }
        let file = try FileHandle(forReadingFrom: fileURL)
        defer { try? file.close() }
        func check() throws { if cancelled() { throw ZGAssetError.cancelled } }
        try check()
        if source.codec == 0 {
            try file.seek(toOffset: UInt64(source.offset + range.lowerBound))
            var remaining = range.upperBound - range.lowerBound
            while remaining > 0 {
                try check()
                guard let data = try file.read(upToCount: Int(min(Int64(chunkSize), remaining))), !data.isEmpty else { throw ZGAssetError.truncated }
                remaining -= Int64(data.count)
                guard emit(data) else { throw ZGAssetError.cancelled }
            }
            return
        }
        var state = z_stream()
        let initialized = inflateInit2_(&state, source.codec == 1 ? 31 : 15,
                                       ZLIB_VERSION, Int32(MemoryLayout<z_stream>.size))
        guard initialized == Z_OK else { throw ZGAssetError.corruptCompressedData(initialized) }
        defer { inflateEnd(&state) }
        try file.seek(toOffset: UInt64(source.offset))
        var remaining = source.storedSize, decoded: Int64 = 0, ended = false
        var output = [UInt8](repeating: 0, count: chunkSize)
        while remaining > 0 && !ended {
            try check()
            guard let input = try file.read(upToCount: Int(min(Int64(chunkSize), remaining))), !input.isEmpty else { throw ZGAssetError.truncated }
            remaining -= Int64(input.count)
            try input.withUnsafeBytes { raw in
                state.next_in = UnsafeMutablePointer(mutating: raw.bindMemory(to: Bytef.self).baseAddress!)
                state.avail_in = uInt(input.count)
                repeat {
                    try check()
                    var status: Int32 = Z_OK
                    var produced = 0
                    var selected: Data?
                    output.withUnsafeMutableBytes { buffer in
                        state.next_out = buffer.bindMemory(to: Bytef.self).baseAddress!
                        state.avail_out = uInt(chunkSize)
                        status = inflate(&state, Z_NO_FLUSH)
                        produced = chunkSize - Int(state.avail_out)
                        let from = max(range.lowerBound, decoded), to = min(range.upperBound, decoded + Int64(produced))
                        if to > from {
                            selected = Data(bytes: buffer.baseAddress!.advanced(by: Int(from - decoded)), count: Int(to - from))
                        }
                    }
                    guard status == Z_OK || status == Z_STREAM_END || status == Z_BUF_ERROR else { throw ZGAssetError.corruptCompressedData(status) }
                    decoded += Int64(produced)
                    guard decoded <= source.size else { throw ZGAssetError.invalidManifest }
                    if let selected, !emit(selected) { throw ZGAssetError.cancelled }
                    if status == Z_STREAM_END { ended = true; break }
                    if range.upperBound < source.size && decoded >= range.upperBound { return }
                    if produced == 0 && state.avail_in == 0 { break }
                    if produced == 0 && status == Z_BUF_ERROR { throw ZGAssetError.truncated }
                } while state.avail_in > 0 || state.avail_out == 0
            }
            // A partial range needs no further decode or disk reads. Full
            // representations continue through the member's native CRC/end.
            if range.upperBound < source.size && decoded >= range.upperBound { return }
        }
        guard ended, decoded == source.size, remaining == 0, state.avail_in == 0 else { throw ZGAssetError.truncated }
    }
}
