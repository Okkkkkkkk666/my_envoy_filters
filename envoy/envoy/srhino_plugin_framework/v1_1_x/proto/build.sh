#!/bin/bash
PROTO_DIR=$(cd $(dirname $0) && pwd)
SRHINO_PLUGIN_FRAMEWORK_DIR=$(cd ${PROTO_DIR}/.. && pwd)
ROOT_DIR=$(cd ${SRHINO_PLUGIN_FRAMEWORK_DIR}/../.. && pwd)

if [ "$1" == "clean" ]; then
    rm -f $(find ${PROTO_DIR} -name *.pb.h | xargs)
    rm -f $(find ${PROTO_DIR} -name *.pb.validate.h | xargs)
    rm -f $(find ${PROTO_DIR} -name *.pb.cc | xargs)
    rm -f $(find ${PROTO_DIR} -name *.pb.validate.cc | xargs)
else
    protoc --cpp_out ${ROOT_DIR} -I ${ROOT_DIR} -I ${PROTO_DIR} $(find ${PROTO_DIR} -name *.proto ! -name validate.proto | xargs) --validate_out="lang=cc:${ROOT_DIR}"
fi
