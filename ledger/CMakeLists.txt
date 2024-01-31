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

ADD_LIBRARY(ledger_objs OBJECT ${ledger_srcs})
if (BUILD_STATIC_LIBS)
  ADD_LIBRARY(ledger STATIC $<TARGET_OBJECTS:ledger_objs>)
else()
  ADD_LIBRARY(ledger SHARED $<TARGET_OBJECTS:ledger_objs>)
endif()

LIST(APPEND LEDGER_LINKER_LIBS tbb)
TARGET_LINK_LIBRARIES(ledger ${LEDGER_LINKER_LIBS})
