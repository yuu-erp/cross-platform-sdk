#include "core/application/logger.h"
#include "support/core/recording_logger.h"

#include <gtest/gtest.h>

namespace fiaisdk::core {
namespace {

TEST(Logger, WritesRecordsWithItsCategory) {
    test::RecordingLogger sink;
    const Logger log(sink, "wallet");
    log.info("created");
    log.error("failed");

    const auto entries = sink.entries();
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries[0].level, LogLevel::Info);
    EXPECT_EQ(entries[0].category, "wallet");
    EXPECT_EQ(entries[0].message, "created");
    EXPECT_EQ(entries[1].level, LogLevel::Error);
}

TEST(Logger, SkipsLevelsTheSinkDisables) {
    test::RecordingLogger sink(LogLevel::Warning);
    const Logger log(sink, "core");
    log.debug("hidden");
    log.warning("shown");
    EXPECT_FALSE(log.isEnabled(LogLevel::Info));
    ASSERT_EQ(sink.entries().size(), 1U);
    EXPECT_EQ(sink.entries()[0].message, "shown");
}

} // namespace
} // namespace fiaisdk::core
