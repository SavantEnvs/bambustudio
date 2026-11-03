#pragma once
#include <boost/log/core.hpp>

// BambuStudio reports parse problems through BOOST_LOG_TRIVIAL; keep the fuzzers' stdout/stderr quiet.
namespace {
struct QuietLog {
    QuietLog() { boost::log::core::get()->set_logging_enabled(false); }
} quiet_log_instance;
} // namespace
