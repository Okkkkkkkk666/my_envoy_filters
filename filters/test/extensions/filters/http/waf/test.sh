bazel test --jobs 4 --copt="-O0" --copt="-ggdb" --strip=never --verbose_failures //filters/test/extensions/filters/http/waf:filter_test --test_output=all


sudo apt intstall lcov
sudo apt install clang llvm

bazel coverage -s --instrument_test_targets=true --instrumentation_filter="/envoy[/:]" --combined_report=lcov //filters/test/extensions/filters/http/waf:filter_test