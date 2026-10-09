#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace discord_member_info {

struct DiscordRole {
    std::string id;
    std::string name;
    std::int32_t position = 0;
    std::int32_t color = 0;
    bool hoist = false;
    bool managed = false;
    bool mentionable = false;
};

struct DiscordMemberInfo {
    bool success = false;
    bool member = false;
    std::string discordId;
    std::string pfpUrl;
    std::string username;
    std::string displayName;
    std::string globalDisplayName;
    std::vector<DiscordRole> roles;
    bool hasHighestRole = false;
    DiscordRole highestRole;
    std::string error;
};

struct DiscordMemberInfoConfig {
    std::wstring endpoint;
    std::string pinnedTransportPublicKeyBase64url;
    std::string pinnedServerPublicKeyPem;
    std::string expectedSigningKeyId;
    std::string expectedIssuer = "crisis-auth-discord-member-info";
    std::string expectedAudience = "windows-client";
    std::string expectedGuildId;
};

class DiscordMemberInfoClient {
public:
    explicit DiscordMemberInfoClient( DiscordMemberInfoConfig config );

    DiscordMemberInfo GetDiscordMemberInfo( const std::string& licenseKey, const std::string& hwid, const std::string& discordId ) const;

private:
    DiscordMemberInfoConfig config_;
};

}
