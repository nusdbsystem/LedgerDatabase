INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

AUX_SOURCE_DIRECTORY(common common_srcs)
LIST(APPEND ledger_srcs ${common_srcs})

AUX_SOURCE_DIRECTORY(ledgerdb ledgerdb_srcs)
LIST(APPEND ledger_srcs ${ledgerdb_srcs})

AUX_SOURCE_DIRECTORY(ledgerdb/mpt mpt_srcs)
LIST(APPEND ledger_srcs ${mpt_srcs})

AUX_SOURCE_DIRECTORY(ledgerdb/skiplist skiplist_srcs)
LIST(APPEND ledger_srcs ${skiplist_srcs})

AUX_SOURCE_DIRECTORY(qldb qldb_srcs)
LIST(APPEND ledger_srcs ${qldb_srcs})

AUX_SOURCE_DIRECTORY(sqlledger sqlledger_srcs)
LIST(APPEND ledger_srcs ${sqlledger_srcs})

AUX_SOURCE_DIRECTORY(ustore ustore_srcs)
LIST(APPEND ledger_srcs ${ustore_srcs})

AUX_SOURCE_DIRECTORY(ustore/node ustore_node_srcs)
LIST(APPEND ledger_srcs ${ustore_node_srcs})

AUX_SOURCE_DIRECTORY(ustore/store ustore_store_srcs)
LIST(APPEND ledger_srcs ${ustore_store_srcs})

AUX_SOURCE_DIRECTORY(ustore/type ustore_type_srcs)
LIST(APPEND ledger_srcs ${ustore_type_srcs})

AUX_SOURCE_DIRECTORY(ustore/type/server ustore_type_server_srcs)
LIST(APPEND ledger_srcs ${ustore_type_server_srcs})

AUX_SOURCE_DIRECTORY(ustore/utils ustore_utils_srcs)
LIST(APPEND ledger_srcs ${ustore_utils_srcs})

AUX_SOURCE_DIRECTORY(ustore/chunk ustore_chunk_srcs)
LIST(APPEND ledger_srcs ${ustore_chunk_srcs})

ADD_LIBRARY(ledger_objs OBJECT ${ledger_srcs})
ADD_LIBRARY(ledger SHARED $<TARGET_OBJECTS:ledger_objs>)

LIST(APPEND LEDGER_LINKER_LIBS tbb)
TARGET_LINK_LIBRARIES(ledger ${LEDGER_LINKER_LIBS})
