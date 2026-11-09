#include "modules/network/domain/endpoint.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace fiaisdk::network {
namespace {

Endpoint parsed(const std::string &url) {
    auto result = Endpoint::parse(url);
    EXPECT_TRUE(result) << url << ": " << (result ? "" : result.error().message);
    return std::move(result).value();
}

void expectInvalid(const std::string &url) {
    const auto result = Endpoint::parse(url);
    ASSERT_FALSE(result) << url << " should be rejected";
    EXPECT_EQ(result.error().code, core::ErrorCode::InvalidArgument) << url;
}

TEST(Endpoint, ParsesHttpsWithDefaultPort) {
    const Endpoint e = parsed("https://node.example.com");
    EXPECT_EQ(e.scheme(), Endpoint::Scheme::Https);
    EXPECT_EQ(e.host(), "node.example.com");
    EXPECT_EQ(e.port(), 443);
    EXPECT_TRUE(e.isSecure());
    EXPECT_EQ(e.url(), "https://node.example.com");
}

TEST(Endpoint, KeepsExplicitPortAndTarget) {
    const Endpoint e = parsed("wss://node.example.com:8546/ws?apikey=SECRET");
    EXPECT_EQ(e.scheme(), Endpoint::Scheme::Wss);
    EXPECT_EQ(e.port(), 8546);
    EXPECT_EQ(e.target(), "/ws?apikey=SECRET");
    EXPECT_EQ(e.url(), "wss://node.example.com:8546/ws?apikey=SECRET");
}

TEST(Endpoint, OriginNeverContainsPathOrQuery) {
    const Endpoint e = parsed("https://node.example.com:8443/rpc/KEY?token=SECRET");
    EXPECT_EQ(e.origin(), "https://node.example.com:8443");
}

TEST(Endpoint, NormalisesSchemeAndHostCaseAndDefaultPort) {
    const Endpoint e = parsed("HTTPS://Node.Example.COM:443/Path");
    EXPECT_EQ(e.url(), "https://node.example.com/Path"); // path case preserved
    EXPECT_EQ(e, parsed("https://node.example.com/Path"));
}

TEST(Endpoint, AllowsPlainHttpOnlyForLoopback) {
    EXPECT_EQ(parsed("http://localhost:8545").port(), 8545);
    EXPECT_EQ(parsed("ws://127.0.0.1").port(), 80);
    EXPECT_EQ(parsed("http://[::1]:8080").host(), "[::1]");
    EXPECT_EQ(parsed("HTTP://LOCALHOST:8545").host(), "localhost");
    expectInvalid("http://node.example.com");
    expectInvalid("ws://10.0.0.5:8546");
    expectInvalid("http://app.localhost"); // resolver-dependent: not treated as loopback
}

TEST(Endpoint, LoopbackLookalikesCannotUsePlainHttp) {
    expectInvalid("http://localhost.evil.com");
    expectInvalid("http://evil.localhost.com");
    expectInvalid("http://127.0.0.1.nip.io");
    expectInvalid("http://evil.com#@localhost");
    expectInvalid("http://evil.com?.localhost");
    expectInvalid("http://evil.com/.localhost");
}

TEST(Endpoint, AcceptsIpv6Literals) {
    const Endpoint e = parsed("https://[2001:db8::1]:8443");
    EXPECT_EQ(e.host(), "[2001:db8::1]");
    EXPECT_EQ(e.port(), 8443);
}

TEST(Endpoint, RejectsMalformedUrls) {
    expectInvalid("");
    expectInvalid("node.example.com");                   // no scheme
    expectInvalid("ftp://node.example.com");             // unsupported scheme
    expectInvalid("https://");                           // no host
    expectInvalid("https://user:pass@node.example.com"); // credentials
    expectInvalid("https://node.example.com:0");
    expectInvalid("https://node.example.com:65536");
    expectInvalid("https://node.example.com:http");
    expectInvalid("https://node.example.com:");
    expectInvalid("https://node..example.com");
    expectInvalid("https://node_example.com");
    expectInvalid("https://node.example.com/a b"); // whitespace
    expectInvalid("https://[::1");                 // unterminated IPv6
    expectInvalid("https://" + std::string(Endpoint::kMaxLength, 'a'));
}

TEST(Endpoint, RejectsAuthorityTricks) {
    expectInvalid("https://a.example.com:443@evil.com");
    expectInvalid("https://evil.com\\@good.com");
    expectInvalid("https://[::1]x");
    expectInvalid("https://[::1]:");
}

TEST(Endpoint, EnforcesDnsLabelSyntax) {
    expectInvalid("https://-a.example.com");
    expectInvalid("https://a-.example.com");
    expectInvalid("https://a.example.com.");                           // trailing dot
    expectInvalid("https://" + std::string(64, 'a') + ".example.com"); // label > 63
    EXPECT_EQ(parsed("https://" + std::string(63, 'a') + ".example.com").port(), 443);
    EXPECT_EQ(parsed("https://my-node.example.com").host(), "my-node.example.com");
}

TEST(Endpoint, RejectsNonAsciiHosts) {
    expectInvalid("https://n\xC3\xB6"
                  "de.example.com");              // UTF-8 "ö": IDN must be punycode
    expectInvalid("https://node.ex\xE4mple.com"); // Latin-1 byte
}

} // namespace
} // namespace fiaisdk::network
