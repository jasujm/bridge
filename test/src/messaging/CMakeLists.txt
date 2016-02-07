add_bridge_test(
  TARGET test-messaging
  NAME TestMessaging
  DEPENDS bridge-messaging bridge-scoring bridge-bridge ${ZEROMQ_LIBRARIES})
