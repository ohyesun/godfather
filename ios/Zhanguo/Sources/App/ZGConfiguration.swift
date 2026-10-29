import Foundation

struct ZGConfiguration: Decodable {
    let profile: ZGConnectionProfile
    let launcherAuthenticationBase64: String
    let baseRevision: String

    var launcherAuthentication: Data {
        Data(base64Encoded: launcherAuthenticationBase64) ?? Data()
    }

    static func load(bundle: Bundle = .main) throws -> ZGConfiguration {
        guard let url = bundle.url(forResource: "NativeConfig", withExtension: "json") else {
            throw ZGConfigurationError.missingConfiguration
        }
        let value = try JSONDecoder().decode(Self.self, from: Data(contentsOf: url))
        try value.profile.validate()
        guard value.baseRevision == "r119", !value.launcherAuthentication.isEmpty,
              value.launcherAuthentication.count <= 1024 else {
            throw ZGConfigurationError.missingConfiguration
        }
        return value
    }
}

enum ZGConfigurationError: LocalizedError {
    case missingConfiguration, missingAssets, missingBootstrap
    var errorDescription: String? { "游戏文件不完整，请使用完整安装包。" }
}
