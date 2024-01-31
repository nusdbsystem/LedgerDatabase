FILE(GLOB dist_proto "proto/*.proto")
protobuf_generate_cpp(dist_proto_srcs dist_proto_hdrs ${dist_proto})
LIST(APPEND dist_srcs ${dist_proto_srcs})

ADD_CUSTOM_TARGET(
  copy_dist_protobuf ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/distributed/proto"
  DEPENDS ${dist_proto_hdrs}
  COMMENT "Copying Protobuf headers"
)
FOREACH(fil ${dist_proto_hdrs})
  GET_FILENAME_COMPONENT(filename ${fil} NAME)
  ADD_CUSTOM_COMMAND(
    TARGET copy_dist_protobuf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${fil}
      "${CMAKE_BINARY_DIR}/distributed/proto/${filename}"
    DEPENDS ${dist_proto}
  )
ENDFOREACH()

AUX_SOURCE_DIRECTORY(timeserver timeserver_srcs)
LIST(APPEND dist_srcs ${timeserver_srcs})

AUX_SOURCE_DIRECTORY(replication replication_srcs)
LIST(APPEND dist_srcs ${replication_srcs})

AUX_SOURCE_DIRECTORY(replication/common replication_common_srcs)
LIST(APPEND dist_srcs ${replication_common_srcs})

AUX_SOURCE_DIRECTORY(replication/vr replication_vr_srcs)
LIST(APPEND dist_srcs ${replication_vr_srcs})

AUX_SOURCE_DIRECTORY(store/common store_common_srcs)
LIST(APPEND dist_srcs ${store_common_srcs})

AUX_SOURCE_DIRECTORY(store/common/backend store_common_backend_srcs)
LIST(APPEND dist_srcs ${store_common_backend_srcs})

AUX_SOURCE_DIRECTORY(store/common/frontend store_common_frontend_srcs)
LIST(APPEND dist_srcs ${store_common_frontend_srcs})

AUX_SOURCE_DIRECTORY(store/strongstore strongstore_srcs)
LIST(APPEND dist_srcs ${strongstore_srcs})

AUX_SOURCE_DIRECTORY(lib lib_srcs)
LIST(APPEND dist_srcs ${lib_srcs})

ADD_LIBRARY(dist_objs OBJECT ${dist_srcs})
ADD_DEPENDENCIES(dist_objs copy_dist_protobuf)

if (BUILD_STATIC_LIBS)
  ADD_LIBRARY(dist STATIC $<TARGET_OBJECTS:dist_objs>)
else()
  ADD_LIBRARY(dist SHARED $<TARGET_OBJECTS:dist_objs>)
endif()
LIST(APPEND DIST_LINKER_LIBS event_openssl)
LIST(APPEND DIST_LINKER_LIBS tbb)
LIST(APPEND DIST_LINKER_LIBS crypto)
LIST(APPEND DIST_LINKER_LIBS ${PROTOBUF_LIBRARY})
TARGET_LINK_LIBRARIES(dist ${DIST_LINKER_LIBS})

ADD_EXECUTABLE(timeserver "exes/timeserver.cc")
ADD_DEPENDENCIES(timeserver copy_dist_protobuf)
ADD_DEPENDENCIES(timeserver ledger)
ADD_DEPENDENCIES(timeserver dist)
TARGET_LINK_LIBRARIES(timeserver dist ledger)
SET_TARGET_PROPERTIES(timeserver PROPERTIES LINK_FLAGS "${LINK_FLAGS}")

ADD_EXECUTABLE(strongstore "exes/strong-server.cc")
ADD_DEPENDENCIES(strongstore copy_dist_protobuf)
ADD_DEPENDENCIES(strongstore dist)
ADD_DEPENDENCIES(strongstore ledger)
TARGET_LINK_LIBRARIES(strongstore dist ledger)
SET_TARGET_PROPERTIES(strongstore PROPERTIES LINK_FLAGS "${LINK_FLAGS}")

ADD_EXECUTABLE(verifyClient "exes/verifyClient.cc")
ADD_DEPENDENCIES(verifyClient copy_dist_protobuf)
ADD_DEPENDENCIES(verifyClient dist)
ADD_DEPENDENCIES(verifyClient ledger)
TARGET_LINK_LIBRARIES(verifyClient dist ledger)
SET_TARGET_PROPERTIES(verifyClient PROPERTIES LINK_FLAGS "${LINK_FLAGS}")

ADD_EXECUTABLE(smallbankClient "exes/smallbankClient.cc")
ADD_DEPENDENCIES(smallbankClient copy_dist_protobuf)
ADD_DEPENDENCIES(smallbankClient dist)
ADD_DEPENDENCIES(smallbankClient ledger)
TARGET_LINK_LIBRARIES(smallbankClient dist ledger)
SET_TARGET_PROPERTIES(smallbankClient PROPERTIES LINK_FLAGS "${LINK_FLAGS}")

ADD_EXECUTABLE(tpccClient "exes/tpccClient.cc")
ADD_DEPENDENCIES(tpccClient copy_dist_protobuf)
ADD_DEPENDENCIES(tpccClient dist)
ADD_DEPENDENCIES(tpccClient ledger)
TARGET_LINK_LIBRARIES(tpccClient dist ledger)
SET_TARGET_PROPERTIES(tpccClient PROPERTIES LINK_FLAGS "${LINK_FLAGS}")

ADD_EXECUTABLE(rangeClient "exes/rangeClient.cc")
ADD_DEPENDENCIES(rangeClient copy_dist_protobuf)
ADD_DEPENDENCIES(rangeClient dist)
ADD_DEPENDENCIES(rangeClient ledger)
TARGET_LINK_LIBRARIES(rangeClient dist ledger)
SET_TARGET_PROPERTIES(rangeClient PROPERTIES LINK_FLAGS "${LINK_FLAGS}")

ADD_EXECUTABLE(auditClient "exes/auditClient.cc")
ADD_DEPENDENCIES(auditClient copy_dist_protobuf)
ADD_DEPENDENCIES(auditClient dist)
ADD_DEPENDENCIES(auditClient ledger)
TARGET_LINK_LIBRARIES(auditClient dist ledger)
SET_TARGET_PROPERTIES(rangeClient PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
