add_bridge_test(
  TARGET test-csmain
  NAME TestCardServerMain
  DEPENDS bridge-csmain bridge-messaging ${ZEROMQ_LIBRARIES})
