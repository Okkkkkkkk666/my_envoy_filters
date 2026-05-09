load(
    ":srhino_test.bzl",
    _srhino_cc_test = "srhino_cc_test",
)
load(
    ":srhino_binary.bzl",
    _srhino_cc_binary = "srhino_cc_binary",
)

load(
    ":srhino_library.bzl",
    _srhino_cc_library = "srhino_cc_library",
)

srhino_cc_test = _srhino_cc_test
srhino_cc_binary = _srhino_cc_binary
srhino_cc_library = _srhino_cc_library
