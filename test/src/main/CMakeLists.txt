add_bridge_test(
  TARGET test-main
  NAME TestMain
  DEPENDS bridge-main bridge-messaging bridge-bridge ${ZEROMQ_LIBRARIES})
