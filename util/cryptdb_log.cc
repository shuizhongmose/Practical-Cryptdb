#include <util/cryptdb_log.hh>

uint64_t cryptdb_logger::enable_mask = cryptdb_logger::mask(log_group::log_debug) | cryptdb_logger::mask(log_group::log_cdb_v);